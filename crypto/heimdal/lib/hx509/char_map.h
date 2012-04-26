#define Q_CONTROL_CHAR		1
#define Q_PRINTABLE		2
#define Q_RFC2253_QUOTE_FIRST	4
#define Q_RFC2253_QUOTE_LAST	8
#define Q_RFC2253_QUOTE		16
#define Q_RFC2253_HEX		32

#define Q_RFC2253		(Q_RFC2253_QUOTE_FIRST|Q_RFC2253_QUOTE_LAST|Q_RFC2253_QUOTE|Q_RFC2253_HEX)



unsigned char char_map[] = {
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x06 ,  0x00 ,  0x00 ,  0x10 ,  0x00 ,  0x00 ,  0x00 ,  0x00 ,
	0x00 ,  0x00 ,  0x00 ,  0x12 ,  0x12 ,  0x02 ,  0x02 ,  0x02 ,
	0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,
	0x02 ,  0x02 ,  0x02 ,  0x10 ,  0x10 ,  0x12 ,  0x10 ,  0x02 ,
	0x00 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,
	0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,
	0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,
	0x02 ,  0x02 ,  0x02 ,  0x00 ,  0x00 ,  0x00 ,  0x00 ,  0x00 ,
	0x00 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,
	0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,
	0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,  0x02 ,
	0x02 ,  0x02 ,  0x02 ,  0x00 ,  0x00 ,  0x00 ,  0x00 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,
	0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21 ,  0x21
};
