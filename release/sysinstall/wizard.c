/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: wizard.c,v 1.13 1998/10/13 09:45:59 jkh Exp $
 *
 */

#include "sysinstall.h"
#include <fcntl.h>
#include <err.h>

int
scan_block(int fd, daddr_t block)
{
    u_char foo[512];

    if (-1 == lseek(fd,block * 512,SEEK_SET))
	err(1,"lseek");
    if (512 != read(fd,foo, 512))
	return 1;
    return 0;
}

void
Scan_Disk(Disk *d)
{
    char device[64];
    u_long l;
    int i,j,fd;

    strcpy(device,"/dev/r");
    strcat(device,d->name);

    fd = open(device,O_RDWR);
    if (fd < 0) {
	msgWarn("open(%s) failed", device);
	return;
    }
    for(i=-1,l=0;;l++) {
	j = scan_block(fd,l);
	if (j != i) {
	    if (i == -1) {
		printf("%c: %lu.",j ? 'B' : 'G', l);
		fflush(stdout);
	    } else if (i == 0) {
		printf(".%lu\nB: %lu.",l-1,l);
		fflush(stdout);
	    } else {
		printf(".%lu\nG: %lu.",l-1,l);
		fflush(stdout);
	    }
	    i = j;
	}
    }
    close(fd);
}

void
slice_wizard(Disk *d)
{
    Disk *db;
    char myprompt[BUFSIZ];
    char input[BUFSIZ];
    char *p,*q=0;
    char **cp,*cmds[200];
    int ncmd,i;

    sprintf(myprompt,"%s> ", d->name);
    while(1) {
	printf("--==##==--\n");
	Debug_Disk(d);
	p = CheckRules(d);
	if (p) {
	    printf("%s",p);
	    free(p);
	}
	printf(myprompt);
	fflush(stdout);
	q = p = fgets(input,sizeof(input),stdin);
	if(!p)
	    break;
	for(cp = cmds; (*cp = strsep(&p, " \t\n")) != NULL;)
	    if (**cp != '\0')
		cp++;
	ncmd = cp - cmds;
	if(!ncmd)
	    continue;
	if (!strcasecmp(*cmds,"quit")) { break; }
	if (!strcasecmp(*cmds,"exit")) { break; }
	if (!strcasecmp(*cmds,"q")) { break; }
	if (!strcasecmp(*cmds,"x")) { break; }
	if (!strcasecmp(*cmds,"delete") && ncmd == 2) {
	    printf("delete = %d\n",
		   Delete_Chunk(d,
				(struct chunk *)strtol(cmds[1],0,0)));
	    continue;
	}
	if (!strcasecmp(*cmds,"allfreebsd")) {
	    All_FreeBSD(d, 0);
	    continue;
	}
	if (!strcasecmp(*cmds,"dedicate")) {
	    All_FreeBSD(d, 1);
	    continue;
	}
	if (!strcasecmp(*cmds,"bios") && ncmd == 4) {
	    Set_Bios_Geom(d,
			  strtol(cmds[1],0,0),
			  strtol(cmds[2],0,0),
			  strtol(cmds[3],0,0));
	    continue;
	}
	if (!strcasecmp(*cmds,"list")) {
	    cp = Disk_Names();
	    printf("Disks:");
	    for(i=0;cp[i];i++) {
		printf(" %s",cp[i]);
		free(cp[i]);
	    }
	    free(cp);
	    continue;
	}
	if (!strcasecmp(*cmds,"create") && ncmd == 6) {

	    printf("Create=%d\n",
		   Create_Chunk(d,
				strtol(cmds[1],0,0),
				strtol(cmds[2],0,0),
				strtol(cmds[3],0,0),
				strtol(cmds[4],0,0),
				strtol(cmds[5],0,0)));
	    continue;
	}
	if (!strcasecmp(*cmds,"read")) {
	    db = d;
	    if (ncmd > 1)
		d = Open_Disk(cmds[1]);
	    else
		d = Open_Disk(d->name);
	    if (d)
		Free_Disk(db);
	    else
		d = db;
	    continue;
	}
	if (!strcasecmp(*cmds,"scan")) {
	    Scan_Disk(d);
	    continue;
	}
	if (!strcasecmp(*cmds,"write")) {
	    printf("Write=%d\n",
		   Fake ? 0 : Write_Disk(d));
	    Free_Disk(d);
	    d = Open_Disk(d->name);
	    continue;
	}
	if (strcasecmp(*cmds,"help"))
	    printf("\007ERROR\n");
	printf("CMDS:\n");
	printf("allfreebsd\t\t");
	printf("dedicate\t\t");
	printf("bios cyl hd sect\n");
	printf("collapse [pointer]\t\t");
	printf("create offset size enum subtype flags\n");
	printf("subtype(part): swap=1, ffs=7\t\t");
	printf("delete pointer\n");
	printf("list\t\t");
	printf("quit\n");
	printf("read [disk]\t\t");
	printf("scan\n");
	printf("write\t\t");
	printf("ENUM:\n\t");
	for(i=0;chunk_n[i];i++)
	    printf("%d = %s%s",i,chunk_n[i],i == 4 ? "\n\t" : "  ");
	printf("\n");

    }
}
