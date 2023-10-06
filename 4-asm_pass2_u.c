/***********************************************************************/
/*  Program Name: 4-asm_pass2_u.c                                      */
/*  This program is the part of SIC/XE assembler Pass 1.	  		   */
/*  The program only identify the symbol, opcode and operand 		   */
/*  of a line of the asm file. The program do not build the            */
/*  SYMTAB.			                                               	   */
/*  2019.12.13                                                         */
/*  2021.03.26 Process error: format 1 & 2 instruction use + 		   */
/*	D1093174			
/*  2023.04.23  MARK TSAI : add for PASS2 							   */
/***********************************************************************/
#include <string.h>
#include "3-asm_pass1_u.c"
#include <stdlib.h>

#define isStrEq(a,b) !strcmp(a,b) 
#define sSIZE 30
#define tBufferSIZE 256
#define rSIZE 0x1E

void printPObject()
{
	int i;
	for(i = 0; i < numofline; i++)
	{
		printf("%06X %12s %12s %12s,%12s (FMT=%X, ADDR=%X) %-10s\n", line[i].address, line[i].symbol, 
		line[i].op, line[i].operand1, line[i].operand2, line[i].fmt, line[i].addressing, line[i].objectcode);
	}

}

short getRegisterNumeric(char r[])
{
	int i;
    char registers[][3] = {"A", "X", "L", "B", "S", "T", "F", "?", "PC", "SW"};
    for( i = 0; i < sizeof(registers) / sizeof(*registers); i++)
        if(!strcmp(r, registers[i]))
            return i;
    return 0;
}

void parseFormat1(LINE *l)
{
    sprintf(l->objectcode, "%02X", l->code);
}

void parseFormat2(LINE *l)
{
    char s[30], *t;
    
    strcpy(s, l->operand1);
    unsigned short reg = 0, i = 4;
    for(t = strtok(s, ","); t; t = strtok(NULL, ","), i = 0)
        reg |= getRegisterNumeric(t) << i;
    sprintf(l->objectcode, "%02X%02X", l->code, reg);
}

void parseFormat3(LINE *l)
{
	char *s;
	char *t;
    //s = line->op;
    struct {unsigned short value:12;}disp;
    short flag;
    short TA;
    
    short PC = l->address +3;
    t = l->operand1;   
	TA=getSymLoc(l);
 

    if(l->code == 0x4C) // only exception;
    {
        sprintf(l->objectcode, "%03X%03X", l->code << 4 | 3 << 4, 0);
        return;
    }
    
    switch(l->addressing)
    {
        case ADDR_IMMEDIATE:
            
            flag |= 1 << 4; //set i
            if(t[0] < 'A')
            {
                sscanf(t, "%3hX", &TA);
                sprintf(l->objectcode, "%03X%03X", l->code << 4 | flag, TA);
                return;
            }
            break;
        case ADDR_INDIRECT:
            flag |= 1 << 5; //set n
            break;
        default:
            flag |= 3 << 4; //set n, i
            break;
    }
    
    for(int i = 0; i < strlen(t); i++)
        if(t[i] == ',')
        flag |= 1 << 3;

    if(TA - PC < 1<<11 && TA - PC > -1<<11)
    {
        disp.value = TA - PC;
        flag |= 1 << 1; //set p
    }
    else if(TA - BASE < 1<<12)
    {
        disp.value = TA - BASE;
        flag |= 1 << 2; //set b
    }
    else
        disp.value = 0; // invalid, throw an error
    sprintf(l->objectcode, "%03X%03X", l->code << 4 | flag, disp.value);
 
}

void parseFormat4(LINE *l)
{
	char *s = l->op;  
    char *t = l->operand1;
    short flag;
    short TA;
    
    flag |= 1; // set e=1
	TA=getSymLoc(l);

    switch(l->addressing)
    {
        case ADDR_IMMEDIATE:
            flag |= 1 << 4; //set i=1
        	if(t[0] < 'A')
            	sscanf(t, "%05hd", &TA);
            break;
            
        default:
            flag |= 3 << 4; //set n=1 i=1
        	if(t[0] < 'A')
            	sscanf(t, "%05hX", &TA);
            break;
    }
    sprintf(l->objectcode, "%03X%05X", l->code << 4 | flag, TA);
}

pass2()
{
	/* process PASS2 base the out form pass1 in the LINE[] structure and generate the object code */
	int i;
	LINE *l;
	char charBuffer[sSIZE], a[sSIZE];
	short b;
	
	for(i = 0; i < numofline; i++)
	{
		l=&line[i];
		
		switch(l->fmt){
			
		case FMT1:
			parseFormat1(l);
			break;
			
		case FMT2:
			parseFormat2(l);
			break;
			
		case FMT3:
			parseFormat3(l);
			break;
				
		case FMT4:
			parseFormat4(l);
			break;	
		}
	if(isStrEq(l->op, "BYTE"))
            switch(l->operand1[0]) // maybe i should pack this
            {
                case 'X':
                    sscanf(l->operand1, "X'%hx'", &b);
                    sprintf(l->objectcode, "%02X", b); //cheap fix
                    break;
                case 'C':                    
                    sscanf(l->operand1, "C'%[^']'", a);
                    for(int j = 0; a[j] != '\0'; j++)
                    {
                        sprintf(charBuffer, "%02X", a[j]);
                        strcat(l->objectcode, charBuffer);
                    }
                    break;
            }
            else if(isStrEq(l->op, "WORD"))
            {
                sscanf(l->operand1, "%hd", &b);
                sprintf(l->objectcode, "%03X", b);
            }	
	}
}

void hRECORD(LINE *p)
{
    char out[tBufferSIZE];
    sprintf(out, "H^%-6s^%06X^%06X", p->symbol, p->address, programlength);
    printf("%s\n", out);
}

void tRECORD(LINE *p)
{
   // operation *s = p->op;
    char tBuffer[tBufferSIZE], out[tBufferSIZE];
    short tBufferLoc = p->address, tBufferLen = 0, len;

    memset(tBuffer, 0, sizeof tBuffer);
    while(!isStrEq(p++->op, "END"))
    {
        if(isStrEq(p->op, "BASE")) continue; //cheap fix, very cheap
        len = strlen(p->objectcode) / 2;
        // l = s->opwidth;
        if((tBufferLen + len > rSIZE || !len) && tBufferLen) //rSIZE is 0x1E
        {
            sprintf(out, "T^%06X^%02X%s", tBufferLoc, tBufferLen, tBuffer);
            memset(tBuffer, tBufferLen = 0, tBufferSIZE); //size of what the pointer points... doesn't work
            printf("%s\n", out);
        }

        if(!len) continue;
        if(!tBufferLen) tBufferLoc = p->address;
        strcat(tBuffer, "^");
        strcat(tBuffer, p->objectcode);
        tBufferLen += len;
    }
}

void mRECORD(LINE *p)
{
    //operation *s = p->op;
    char out[tBufferSIZE];
    while(!isStrEq(p++->op, "END"))
        if(p->fmt == FMT4 && p->addressing != ADDR_IMMEDIATE)
        {
            sprintf(out, "M^%06X^05", p->address + 1);
            printf("%s\n", out);
        }
}

void eRECORD(LINE *p)
{
    char out[tBufferSIZE];
    sprintf(out, "E^%06X", start_locctr);
    printf("%s\n", out);
}

void genHTE(LINE *l) //generate HTE Record
{
    hRECORD(l);
    tRECORD(l);
    mRECORD(l);
    eRECORD(l);
}
int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		printf("Usage: %s fname.asm\n", argv[0]);
	}
	else
	{
		pass1(argv[1]);
		//printPObject();		
		pass2();	
	}
	printPObject();
	printf("\n\nProgram length = %X", locctr-start_locctr);
	printf("\nSYMBOL TABLE:\n");
	printsymbol();
	printf("\nObject program:\n");
	genHTE(&line[0]);
}
