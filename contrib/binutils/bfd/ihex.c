/* BFD back-end for Intel Hex objects.
   Copyright 1995, 1996, 1998, 1999, 2000 Free Software Foundation, Inc.
   Written by Ian Lance Taylor of Cygnus Support <ian@cygnus.com>.

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* This is what Intel Hex files look like:

1. INTEL FORMATS

A. Intel 1

   16-bit address-field format, for files 64k bytes in length or less.

   DATA RECORD
   Byte 1	Header = colon(:)
   2..3		The number of data bytes in hex notation
   4..5		High byte of the record load address
   6..7		Low byte of the record load address
   8..9		Record type, must be "00"
   10..x	Data bytes in hex notation:
	x = (number of bytes - 1) * 2 + 11
   x+1..x+2	Checksum in hex notation
   x+3..x+4	Carriage return, line feed

   END RECORD
   Byte 1	Header = colon (:)
   2..3		The byte count, must be "00"
   4..7		Transfer-address (usually "0000")
		the jump-to address, execution start address
   8..9		Record type, must be "01"
   10..11	Checksum, in hex notation
   12..13	Carriage return, line feed

B. INTEL 2

   MCS-86 format, using a 20-bit address for files larger than 64K bytes.

   DATA RECORD
   Byte 1	Header = colon (:)
   2..3		The byte count of this record, hex notation
   4..5		High byte of the record load address
   6..7		Low byte of the record load address
   8..9		Record type, must be "00"
   10..x	The data bytes in hex notation:
	x = (number of data bytes - 1) * 2 + 11
   x+1..x+2	Checksum in hex notation
   x+3..x+4	Carriage return, line feed

   EXTENDED ADDRESS RECORD
   Byte 1	Header = colon(:)
   2..3		The byte count, must be "02"
   4..7		Load address, must be "0000"
   8..9		Record type, must be "02"
   10..11	High byte of the offset address
   12..13	Low byte of the offset address
   14..15	Checksum in hex notation
   16..17	Carriage return, line feed

   The checksums are the two's complement of the 8-bit sum
   without carry of the byte count, offset address, and the
   record type.

   START ADDRESS RECORD
   Byte 1	Header = colon (:)
   2..3		The byte count, must be "04"
   4..7		Load address, must be "0000"
   8..9		Record type, must be "03"
   10..13	8086 CS value
   14..17	8086 IP value
   18..19	Checksum in hex notation
   20..21	Carriage return, line feed

Another document reports these additional types:

   EXTENDED LINEAR ADDRESS RECORD
   Byte 1	Header = colon (:)
   2..3		The byte count, must be "02"
   4..7		Load address, must be "0000"
   8..9		Record type, must be "04"
   10..13	Upper 16 bits of address of subsequent records
   14..15	Checksum in hex notation
   16..17	Carriage return, line feed

   START LINEAR ADDRESS RECORD
   Byte 1	Header = colon (:)
   2..3		The byte count, must be "02"
   4..7		Load address, must be "0000"
   8..9		Record type, must be "05"
   10..13	Upper 16 bits of start address
   14..15	Checksum in hex notation
   16..17	Carriage return, line feed

The MRI compiler uses this, which is a repeat of type 5:

  EXTENDED START RECORD
   Byte 1	Header = colon (:)
   2..3		The byte count, must be "04"
   4..7		Load address, must be "0000"
   8..9		Record type, must be "05"
   10..13	Upper 16 bits of start address
   14..17	Lower 16 bits of start address
   18..19	Checksum in hex notation
   20..21	Carriage return, line feed
*/

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "libiberty.h"

#include <ctype.h>

static void ihex_init PARAMS ((void));
static boolean ihex_mkobject PARAMS ((bfd *));
static INLINE int ihex_get_byte PARAMS ((bfd *, boolean *));
static void ihex_bad_byte PARAMS ((bfd *, unsigned int, int, boolean));
static boolean ihex_scan PARAMS ((bfd *));
static const bfd_target *ihex_object_p PARAMS ((bfd *));
static boolean ihex_read_section PARAMS ((bfd *, asection *, bfd_byte *));
static boolean ihex_get_section_contents
  PARAMS ((bfd *, asection *, PTR, file_ptr, bfd_size_type));
static boolean ihex_set_section_contents
  PARAMS ((bfd *, asection *, PTR, file_ptr, bfd_size_type));
static boolean ihex_write_record
  PARAMS ((bfd *, bfd_size_type, bfd_vma, unsigned int, bfd_byte *));
static boolean ihex_write_object_contents PARAMS ((bfd *));
static asymbol *ihex_make_empty_symbol PARAMS ((bfd *));
static boolean ihex_set_arch_mach
  PARAMS ((bfd *, enum bfd_architecture, unsigned long));
static int ihex_sizeof_headers PARAMS ((bfd *, boolean));

/* The number of bytes we put on one line during output.  */

#define CHUNK 16

/* Macros for converting between hex and binary. */

#define NIBBLE(x) (hex_value (x))
#define HEX2(buffer) ((NIBBLE ((buffer)[0]) << 4) + NIBBLE ((buffer)[1]))
#define HEX4(buffer) ((HEX2 (buffer) << 8) + HEX2 ((buffer) + 2))
#define ISHEX(x) (hex_p (x))

/* When we write out an ihex value, the values can not be output as
   they are seen.  Instead, we hold them in memory in this structure.  */

struct ihex_data_list
{
  struct ihex_data_list *next;
  bfd_byte *data;
  bfd_vma where;
  bfd_size_type size;
};

/* The ihex tdata information.  */

struct ihex_data_struct
{
  struct ihex_data_list *head;
  struct ihex_data_list *tail;
};

/* Initialize by filling in the hex conversion array.  */

static void
ihex_init ()
{
  static boolean inited;

  if (! inited)
    {
      inited = true;
      hex_init ();
    }
}

/* Create an ihex object.  */

static boolean
ihex_mkobject (abfd)
     bfd *abfd;
{
  if (abfd->tdata.ihex_data == NULL)
    {
      struct ihex_data_struct *tdata;

      tdata = ((struct ihex_data_struct *)
	       bfd_alloc (abfd, sizeof (struct ihex_data_struct)));
      if (tdata == NULL)
	return false;
      abfd->tdata.ihex_data = tdata;
      tdata->head = NULL;
      tdata->tail = NULL;
    }

  return true;
}

/* Read a byte from a BFD.  Set *ERRORPTR if an error occurred.
   Return EOF on error or end of file.  */

static INLINE int
ihex_get_byte (abfd, errorptr)
     bfd *abfd;
     boolean *errorptr;
{
  bfd_byte c;

  if (bfd_read (&c, 1, 1, abfd) != 1)
    {
      if (bfd_get_error () != bfd_error_file_truncated)
	*errorptr = true;
      return EOF;
    }

  return (int) (c & 0xff);
}

/* Report a problem in an Intel Hex file.  */

static void
ihex_bad_byte (abfd, lineno, c, error)
     bfd *abfd;
     unsigned int lineno;
     int c;
     boolean error;
{
  if (c == EOF)
    {
      if (! error)
	bfd_set_error (bfd_error_file_truncated);
    }
  else
    {
      char buf[10];

      if (! isprint (c))
	sprintf (buf, "\\%03o", (unsigned int) c);
      else
	{
	  buf[0] = c;
	  buf[1] = '\0';
	}
      (*_bfd_error_handler)
	(_("%s:%d: unexpected character `%s' in Intel Hex file\n"),
	 bfd_get_filename (abfd), lineno, buf);
      bfd_set_error (bfd_error_bad_value);
    }
}

/* Read an Intel hex file and turn it into sections.  We create a new
   section for each contiguous set of bytes.  */

static boolean
ihex_scan (abfd)
     bfd *abfd;
{
  bfd_vma segbase;
  bfd_vma extbase;
  asection *sec;
  int lineno;
  boolean error;
  bfd_byte *buf = NULL;
  size_t bufsize;
  int c;

  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
    goto error_return;

  abfd->start_address = 0;

  segbase = 0;
  extbase = 0;
  sec = NULL;
  lineno = 1;
  error = false;
  bufsize = 0;
  while ((c = ihex_get_byte (abfd, &error)) != EOF)
    {
      if (c == '\r')
	continue;
      else if (c == '\n')
	{
	  ++lineno;
	  continue;
	}
      else if (c != ':')
	{
	  ihex_bad_byte (abfd, lineno, c, error);
	  goto error_return;
	}
      else
	{
	  file_ptr pos;
	  char hdr[8];
	  unsigned int i;
	  unsigned int len;
	  bfd_vma addr;
	  unsigned int type;
	  unsigned int chars;
	  unsigned int chksum;

	  /* This is a data record.  */

	  pos = bfd_tell (abfd) - 1;

	  /* Read the header bytes.  */

	  if (bfd_read (hdr, 1, 8, abfd) != 8)
	    goto error_return;

	  for (i = 0; i < 8; i++)
	    {
	      if (! ISHEX (hdr[i]))
		{
		  ihex_bad_byte (abfd, lineno, hdr[i], error);
		  goto error_return;
		}
	    }

	  len = HEX2 (hdr);
	  addr = HEX4 (hdr + 2);
	  type = HEX2 (hdr + 6);

	  /* Read the data bytes.  */

	  chars = len * 2 + 2;
	  if (chars >= bufsize)
	    {
	      buf = (bfd_byte *) bfd_realloc (buf, chars);
	      if (buf == NULL)
		goto error_return;
	      bufsize = chars;
	    }

	  if (bfd_read (buf, 1, chars, abfd) != chars)
	    goto error_return;

	  for (i = 0; i < chars; i++)
	    {
	      if (! ISHEX (buf[i]))
		{
		  ihex_bad_byte (abfd, lineno, hdr[i], error);
		  goto error_return;
		}
	    }

	  /* Check the checksum.  */
	  chksum = len + addr + (addr >> 8) + type;
	  for (i = 0; i < len; i++)
	    chksum += HEX2 (buf + 2 * i);
	  if (((- chksum) & 0xff) != (unsigned int) HEX2 (buf + 2 * i))
	    {
	      (*_bfd_error_handler)
		(_("%s:%d: bad checksum in Intel Hex file (expected %u, found %u)"),
		 bfd_get_filename (abfd), lineno,
		 (- chksum) & 0xff, (unsigned int) HEX2 (buf + 2 * i));
	      bfd_set_error (bfd_error_bad_value);
	      goto error_return;
	    }

	  switch (type)
	    {
	    case 0:
	      /* This is a data record.  */
	      if (sec != NULL
		  && sec->vma + sec->_raw_size == extbase + segbase + addr)
		{
		  /* This data goes at the end of the section we are
                     currently building.  */
		  sec->_raw_size += len;
		}
	      else if (len > 0)
		{
		  char secbuf[20];
		  char *secname;

		  sprintf (secbuf, ".sec%d", bfd_count_sections (abfd) + 1);
		  secname = (char *) bfd_alloc (abfd, strlen (secbuf) + 1);
		  if (secname == NULL)
		    goto error_return;
		  strcpy (secname, secbuf);
		  sec = bfd_make_section (abfd, secname);
		  if (sec == NULL)
		    goto error_return;
		  sec->flags = SEC_HAS_CONTENTS | SEC_LOAD | SEC_ALLOC;
		  sec->vma = extbase + segbase + addr;
		  sec->lma = extbase + segbase + addr;
		  sec->_raw_size = len;
		  sec->filepos = pos;
		}
	      break;

	    case 1:
	      /* An end record.  */
	      if (abfd->start_address == 0)
		abfd->start_address = addr;
	      if (buf != NULL)
		free (buf);
	      return true;

	    case 2:
	      /* An extended address record.  */
	      if (len != 2)
		{
		  (*_bfd_error_handler)
		    (_("%s:%d: bad extended address record length in Intel Hex file"),
		     bfd_get_filename (abfd), lineno);
		  bfd_set_error (bfd_error_bad_value);
		  goto error_return;
		}

	      segbase = HEX4 (buf) << 4;

	      sec = NULL;

	      break;

	    case 3:
	      /* An extended start address record.  */
	      if (len != 4)
		{
		  (*_bfd_error_handler)
		    (_("%s:%d: bad extended start address length in Intel Hex file"),
		     bfd_get_filename (abfd), lineno);
		  bfd_set_error (bfd_error_bad_value);
		  goto error_return;
		}

	      abfd->start_address += (HEX4 (buf) << 4) + HEX4 (buf + 4);

	      sec = NULL;

	      break;

	    case 4:
	      /* An extended linear address record.  */
	      if (len != 2)
		{
		  (*_bfd_error_handler)
		    (_("%s:%d: bad extended linear address record length in Intel Hex file"),
		     bfd_get_filename (abfd), lineno);
		  bfd_set_error (bfd_error_bad_value);
		  goto error_return;
		}

	      extbase = HEX4 (buf) << 16;

	      sec = NULL;

	      break;

	    case 5:
	      /* An extended linear start address record.  */
	      if (len != 2 && len != 4)
		{
		  (*_bfd_error_handler)
		    (_("%s:%d: bad extended linear start address length in Intel Hex file"),
		     bfd_get_filename (abfd), lineno);
		  bfd_set_error (bfd_error_bad_value);
		  goto error_return;
		}

	      if (len == 2)
		abfd->start_address += HEX4 (buf) << 16;
	      else
		abfd->start_address = (HEX4 (buf) << 16) + HEX4 (buf + 4);

	      sec = NULL;

	      break;

	    default:
	      (*_bfd_error_handler)
		(_("%s:%d: unrecognized ihex type %u in Intel Hex file\n"),
		 bfd_get_filename (abfd), lineno, type);
	      bfd_set_error (bfd_error_bad_value);
	      goto error_return;
	    }
	}
    }

  if (error)
    goto error_return;

  if (buf != NULL)
    free (buf);

  return true;

 error_return:
  if (buf != NULL)
    free (buf);
  return false;
}

/* Try to recognize an Intel Hex file.  */

static const bfd_target *
ihex_object_p (abfd)
     bfd *abfd;
{
  bfd_byte b[9];
  unsigned int i;
  unsigned int type;

  ihex_init ();

  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
    return NULL;
  if (bfd_read (b, 1, 9, abfd) != 9)
    {
      if (bfd_get_error () == bfd_error_file_truncated)
	bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if (b[0] != ':')
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  for (i = 1; i < 9; i++)
    {
      if (! ISHEX (b[i]))
	{
	  bfd_set_error (bfd_error_wrong_format);
	  return NULL;
	}
    }

  type = HEX2 (b + 7);
  if (type > 5)
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  /* OK, it looks like it really is an Intel Hex file.  */

  if (! ihex_mkobject (abfd)
      || ! ihex_scan (abfd))
    return NULL;

  return abfd->xvec;
}

/* Read the contents of a section in an Intel Hex file.  */

static boolean
ihex_read_section (abfd, section, contents)
     bfd *abfd;
     asection *section;
     bfd_byte *contents;
{
  int c;
  bfd_byte *p;
  bfd_byte *buf = NULL;
  size_t bufsize;
  boolean error;

  if (bfd_seek (abfd, section->filepos, SEEK_SET) != 0)
    goto error_return;

  p = contents;
  bufsize = 0;
  error = false;
  while ((c = ihex_get_byte (abfd, &error)) != EOF)
    {
      char hdr[8];
      unsigned int len;
      bfd_vma addr;
      unsigned int type;
      unsigned int i;

      if (c == '\r' || c == '\n')
	continue;

      /* This is called after ihex_scan has succeeded, so we ought to
         know the exact format.  */
      BFD_ASSERT (c == ':');

      if (bfd_read (hdr, 1, 8, abfd) != 8)
	goto error_return;

      len = HEX2 (hdr);
      addr = HEX4 (hdr + 2);
      type = HEX2 (hdr + 6);

      /* We should only see type 0 records here.  */
      if (type != 0)
	{
	  (*_bfd_error_handler)
	    (_("%s: internal error in ihex_read_section"),
	     bfd_get_filename (abfd));
	  bfd_set_error (bfd_error_bad_value);
	  goto error_return;
	}

      if (len * 2 > bufsize)
	{
	  buf = (bfd_byte *) bfd_realloc (buf, len * 2);
	  if (buf == NULL)
	    goto error_return;
	  bufsize = len * 2;
	}

      if (bfd_read (buf, 1, len * 2, abfd) != len * 2)
	goto error_return;

      for (i = 0; i < len; i++)
	*p++ = HEX2 (buf + 2 * i);
      if ((bfd_size_type) (p - contents) >= section->_raw_size)
	{
	  /* We've read everything in the section.  */
	  if (buf != NULL)
	    free (buf);
	  return true;
	}

      /* Skip the checksum.  */
      if (bfd_read (buf, 1, 2, abfd) != 2)
	goto error_return;
    }

  if ((bfd_size_type) (p - contents) < section->_raw_size)
    {
      (*_bfd_error_handler)
	(_("%s: bad section length in ihex_read_section"),
	 bfd_get_filename (abfd));
      bfd_set_error (bfd_error_bad_value);
      goto error_return;
    }

  if (buf != NULL)
    free (buf);

  return true;

 error_return:
  if (buf != NULL)
    free (buf);
  return false;
}

/* Get the contents of a section in an Intel Hex file.  */

static boolean
ihex_get_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     asection *section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  if (section->used_by_bfd == NULL)
    {
      section->used_by_bfd = bfd_alloc (abfd, section->_raw_size);
      if (section->used_by_bfd == NULL)
	return false;
      if (! ihex_read_section (abfd, section, section->used_by_bfd))
	return false;
    }

  memcpy (location, (bfd_byte *) section->used_by_bfd + offset,
	  (size_t) count);

  return true;
}

/* Set the contents of a section in an Intel Hex file.  */

static boolean
ihex_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     asection *section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  struct ihex_data_list *n;
  bfd_byte *data;
  struct ihex_data_struct *tdata;

  if (count == 0
      || (section->flags & SEC_ALLOC) == 0
      || (section->flags & SEC_LOAD) == 0)
    return true;

  n = ((struct ihex_data_list *)
       bfd_alloc (abfd, sizeof (struct ihex_data_list)));
  if (n == NULL)
    return false;

  data = (bfd_byte *) bfd_alloc (abfd, count);
  if (data == NULL)
    return false;
  memcpy (data, location, (size_t) count);

  n->data = data;
  n->where = section->lma + offset;
  n->size = count;

  /* Sort the records by address.  Optimize for the common case of
     adding a record to the end of the list.  */
  tdata = abfd->tdata.ihex_data;
  if (tdata->tail != NULL
      && n->where >= tdata->tail->where)
    {
      tdata->tail->next = n;
      n->next = NULL;
      tdata->tail = n;
    }
  else
    {
      register struct ihex_data_list **pp;

      for (pp = &tdata->head;
	   *pp != NULL && (*pp)->where < n->where;
	   pp = &(*pp)->next)
	;
      n->next = *pp;
      *pp = n;
      if (n->next == NULL)
	tdata->tail = n;
    }

  return true;
}

/* Write a record out to an Intel Hex file.  */

static boolean
ihex_write_record (abfd, count, addr, type, data)
     bfd *abfd;
     bfd_size_type count;
     bfd_vma addr;
     unsigned int type;
     bfd_byte *data;
{
  static const char digs[] = "0123456789ABCDEF";
  char buf[9 + CHUNK * 2 + 4];
  char *p;
  unsigned int chksum;
  unsigned int i;

#define TOHEX(buf, v) \
  ((buf)[0] = digs[((v) >> 4) & 0xf], (buf)[1] = digs[(v) & 0xf])

  buf[0] = ':';
  TOHEX (buf + 1, count);
  TOHEX (buf + 3, (addr >> 8) & 0xff);
  TOHEX (buf + 5, addr & 0xff);
  TOHEX (buf + 7, type);

  chksum = count + addr + (addr >> 8) + type;

  for (i = 0, p = buf + 9; i < count; i++, p += 2, data++)
    {
      TOHEX (p, *data);
      chksum += *data;
    }

  TOHEX (p, (- chksum) & 0xff);
  p[2] = '\r';
  p[3] = '\n';

  if (bfd_write (buf, 1, 9 + count * 2 + 4, abfd) != 9 + count * 2 + 4)
    return false;

  return true;
}

/* Write out an Intel Hex file.  */

static boolean
ihex_write_object_contents (abfd)
     bfd *abfd;
{
  bfd_vma segbase;
  bfd_vma extbase;
  struct ihex_data_list *l;

  segbase = 0;
  extbase = 0;
  for (l = abfd->tdata.ihex_data->head; l != NULL; l = l->next)
    {
      bfd_vma where;
      bfd_byte *p;
      bfd_size_type count;

      where = l->where;
      p = l->data;
      count = l->size;
      while (count > 0)
	{
	  bfd_size_type now;

	  now = count;
	  if (now > CHUNK)
	    now = CHUNK;

	  if (where > segbase + extbase + 0xffff)
	    {
	      bfd_byte addr[2];

	      /* We need a new base address.  */
	      if (where <= 0xfffff)
		{
		  /* The addresses should be sorted.  */
		  BFD_ASSERT (extbase == 0);

		  segbase = where & 0xf0000;
		  addr[0] = (bfd_byte)(segbase >> 12) & 0xff;
		  addr[1] = (bfd_byte)(segbase >> 4) & 0xff;
		  if (! ihex_write_record (abfd, 2, 0, 2, addr))
		    return false;
		}
	      else
		{
		  /* The extended address record and the extended
                     linear address record are combined, at least by
                     some readers.  We need an extended linear address
                     record here, so if we've already written out an
                     extended address record, zero it out to avoid
                     confusion.  */
		  if (segbase != 0)
		    {
		      addr[0] = 0;
		      addr[1] = 0;
		      if (! ihex_write_record (abfd, 2, 0, 2, addr))
			return false;
		      segbase = 0;
		    }

		  extbase = where & 0xffff0000;
		  if (where > extbase + 0xffff)
		    {
		      char buf[20];

		      sprintf_vma (buf, where);
		      (*_bfd_error_handler)
			(_("%s: address 0x%s out of range for Intex Hex file"),
			 bfd_get_filename (abfd), buf);
		      bfd_set_error (bfd_error_bad_value);
		      return false;
		    }
		  addr[0] = (bfd_byte)(extbase >> 24) & 0xff;
		  addr[1] = (bfd_byte)(extbase >> 16) & 0xff;
		  if (! ihex_write_record (abfd, 2, 0, 4, addr))
		    return false;
		}
	    }

	  if (! ihex_write_record (abfd, now, where - (extbase + segbase),
				   0, p))
	    return false;

	  where += now;
	  p += now;
	  count -= now;
	}
    }

  if (abfd->start_address != 0)
    {
      bfd_vma start;
      bfd_byte startbuf[4];

      start = abfd->start_address;

      if (start <= 0xfffff)
	{
	  startbuf[0] = (bfd_byte)((start & 0xf0000) >> 12) & 0xff;
	  startbuf[1] = 0;
	  startbuf[2] = (bfd_byte)(start >> 8) & 0xff;
	  startbuf[3] = (bfd_byte)start & 0xff;
	  if (! ihex_write_record (abfd, 4, 0, 3, startbuf))
	    return false;
	}
      else
	{
	  startbuf[0] = (bfd_byte)(start >> 24) & 0xff;
	  startbuf[1] = (bfd_byte)(start >> 16) & 0xff;
	  startbuf[2] = (bfd_byte)(start >> 8) & 0xff;
	  startbuf[3] = (bfd_byte)start & 0xff;
	  if (! ihex_write_record (abfd, 4, 0, 5, startbuf))
	    return false;
	}
    }

  if (! ihex_write_record (abfd, 0, 0, 1, NULL))
    return false;

  return true;
}

/* Make an empty symbol.  This is required only because
   bfd_make_section_anyway wants to create a symbol for the section.  */

static asymbol *
ihex_make_empty_symbol (abfd)
     bfd *abfd;
{
  asymbol *new;

  new = (asymbol *) bfd_zalloc (abfd, sizeof (asymbol));
  if (new != NULL)
    new->the_bfd = abfd;
  return new;
}

/* Set the architecture for the output file.  The architecture is
   irrelevant, so we ignore errors about unknown architectures.  */

static boolean
ihex_set_arch_mach (abfd, arch, mach)
     bfd *abfd;
     enum bfd_architecture arch;
     unsigned long mach;
{
  if (! bfd_default_set_arch_mach (abfd, arch, mach))
    {
      if (arch != bfd_arch_unknown)
	return false;
    }
  return true;
}

/* Get the size of the headers, for the linker.  */

/*ARGSUSED*/
static int
ihex_sizeof_headers (abfd, exec)
     bfd *abfd ATTRIBUTE_UNUSED;
     boolean exec ATTRIBUTE_UNUSED;
{
  return 0;
}

/* Some random definitions for the target vector.  */

#define	ihex_close_and_cleanup _bfd_generic_close_and_cleanup
#define ihex_bfd_free_cached_info _bfd_generic_bfd_free_cached_info
#define ihex_new_section_hook _bfd_generic_new_section_hook
#define ihex_get_section_contents_in_window \
  _bfd_generic_get_section_contents_in_window

#define ihex_get_symtab_upper_bound bfd_0l
#define ihex_get_symtab \
  ((long (*) PARAMS ((bfd *, asymbol **))) bfd_0l)
#define ihex_print_symbol _bfd_nosymbols_print_symbol
#define ihex_get_symbol_info _bfd_nosymbols_get_symbol_info
#define ihex_bfd_is_local_label_name _bfd_nosymbols_bfd_is_local_label_name
#define ihex_get_lineno _bfd_nosymbols_get_lineno
#define ihex_find_nearest_line _bfd_nosymbols_find_nearest_line
#define ihex_bfd_make_debug_symbol _bfd_nosymbols_bfd_make_debug_symbol
#define ihex_read_minisymbols _bfd_nosymbols_read_minisymbols
#define ihex_minisymbol_to_symbol _bfd_nosymbols_minisymbol_to_symbol

#define ihex_get_reloc_upper_bound \
  ((long (*) PARAMS ((bfd *, asection *))) bfd_0l)
#define ihex_canonicalize_reloc \
  ((long (*) PARAMS ((bfd *, asection *, arelent **, asymbol **))) bfd_0l)
#define ihex_bfd_reloc_type_lookup _bfd_norelocs_bfd_reloc_type_lookup

#define ihex_bfd_get_relocated_section_contents \
  bfd_generic_get_relocated_section_contents
#define ihex_bfd_relax_section bfd_generic_relax_section
#define ihex_bfd_gc_sections bfd_generic_gc_sections
#define ihex_bfd_link_hash_table_create _bfd_generic_link_hash_table_create
#define ihex_bfd_link_add_symbols _bfd_generic_link_add_symbols
#define ihex_bfd_final_link _bfd_generic_final_link
#define ihex_bfd_link_split_section _bfd_generic_link_split_section

/* The Intel Hex target vector.  */

const bfd_target ihex_vec =
{
  "ihex",			/* name */
  bfd_target_ihex_flavour,
  BFD_ENDIAN_UNKNOWN,		/* target byte order */
  BFD_ENDIAN_UNKNOWN,		/* target headers byte order */
  0,				/* object flags */
  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD),	/* section flags */
  0,				/* leading underscore */
  ' ',				/* ar_pad_char */
  16,				/* ar_max_namelen */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* hdrs */

  {
    _bfd_dummy_target,
    ihex_object_p,		/* bfd_check_format */
    _bfd_dummy_target,
    _bfd_dummy_target,
  },
  {
    bfd_false,
    ihex_mkobject,
    _bfd_generic_mkarchive,
    bfd_false,
  },
  {				/* bfd_write_contents */
    bfd_false,
    ihex_write_object_contents,
    _bfd_write_archive_contents,
    bfd_false,
  },

  BFD_JUMP_TABLE_GENERIC (ihex),
  BFD_JUMP_TABLE_COPY (_bfd_generic),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
  BFD_JUMP_TABLE_SYMBOLS (ihex),
  BFD_JUMP_TABLE_RELOCS (ihex),
  BFD_JUMP_TABLE_WRITE (ihex),
  BFD_JUMP_TABLE_LINK (ihex),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  NULL,
  
  (PTR) 0
};
