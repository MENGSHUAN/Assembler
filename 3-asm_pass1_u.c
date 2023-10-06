#include <string.h>
#include "2-optable.c"

/* Public variables and functions */ /*用 bit 做運算*/ 
#define	ADDR_SIMPLE			0x01
#define	ADDR_IMMEDIATE		0x02
#define	ADDR_INDIRECT		0x04
#define	ADDR_INDEX			0x08

#define	LINE_EOF			(-1)
#define	LINE_COMMENT		(-2)
#define	LINE_ERROR			(0)
#define	LINE_CORRECT		(1)

#define FOUND				1
#define NOT_FOUND			0
#define SYMBOL_TABLE_SIZE   100

typedef struct
{
	char		symbol[LEN_SYMBOL];
	char		op[LEN_SYMBOL];
	char		operand1[LEN_SYMBOL];
	char		operand2[LEN_SYMBOL];
	unsigned	code;
	unsigned	fmt;
	unsigned	addressing;	/*addressing mode*/
	unsigned	address;
	char 		objectcode[32];
} LINE;

LINE line[1024];
int locctr = 0x0; 
int start_locctr = 0x0;
int numofline;  //總行數 
short BASE;
int programlength;

int process_line(LINE *line); /*會去呼叫前面兩個程式*/ 
/* return LINE_EOF, LINE_COMMENT, LINE_ERROR, LINE_CORRECT and Instruction information in *line*/

/* Private variable and function */
//line初始化
void init_LINE(LINE *line)  
{
	line->symbol[0] = '\0';
	line->op[0] = '\0';
	line->operand1[0] = '\0';
	line->operand2[0] = '\0';
	line->code = 0x0;
	line->fmt = 0x0;
	line->addressing = ADDR_SIMPLE;
}


typedef struct symtab
{
	char symbol[LEN_SYMBOL];
	unsigned loc;
} symtab;

symtab sym[SYMBOL_TABLE_SIZE];
unsigned int start_address = 0;
unsigned int symbol_no = 0;

//find symbol (symbol是否在symbol table中)
int findsym(LINE *line)
{
	int i;
	for (i = 0; i < symbol_no; i++)
		if(!strcmp(sym[i].symbol , line->symbol))
			return FOUND;
	return NOT_FOUND;
}

int getSymLoc(LINE *line)
{
	int i;
	for (i = 0; i < symbol_no; i++)
		if(!strcmp(sym[i].symbol , line->operand1))
			return sym[i].loc;
	return NOT_FOUND;
}

// add new symbol to SYMTAB
void addsym(LINE *line) {
	if(symbol_no == SYMBOL_TABLE_SIZE)
	{
		printf("Symbol table full !!\n");
		return;	
	}	
	sym[symbol_no].loc = locctr; 
	strcpy(sym[symbol_no].symbol, line->symbol); 
	symbol_no++;
}

//print out symtab
int printsymbol()
{
	int i;
	for (i = 0; i < symbol_no; i++)
		printf("%-6s %06X\n", sym[i].symbol, sym[i].loc);		
}

int process_line(LINE *line)
/* return LINE_EOF, LINE_COMMENT, LINE_ERROR, LINE_CORRECT */
{
	char		buf[LEN_SYMBOL];
	int			c;
	int			state;			//狀態機的state 
	int			ret;        	//LINE_CORRECT or LINE_ERROR
	Instruction	*op;        	//查optable 
	
	c = ASM_token(buf);						/* get the first token of a line */
	if(c == EOF)							/* line 結尾 (處理完畢) */
		return LINE_EOF;					
	else if((c == 1) && (buf[0] == '\n'))	/* blank line (空白行也當成註解) */
		return LINE_COMMENT;
	else if((c == 1) && (buf[0] == '.'))	/* a comment line */
	{
		do
		{
			c = ASM_token(buf);
		} while((c != EOF) && (buf[0] != '\n'));
		return LINE_COMMENT;
	}
	else
	{
		init_LINE(line);
		ret = LINE_ERROR;
		state = 0;
		while(state < 8)
		{
			switch(state)
			{
				case 0:
				case 1:
				case 2:
					op = is_opcode(buf); 		//前一個程式的function, 如果是指令 回傳一個非NULL的位置 
					if((state < 2) && (buf[0] == '+'))	/* + */
					{
						line->fmt = FMT4;
						state = 2;
					}
					else if(op != NULL)	/* INSTRUCTION */
					{
						strcpy(line->op, op->op); //op裡的op copy到line裡面的op 
						line->code = op->code;
						state = 3;
						if(line->fmt != FMT4)
						{
							line->fmt = op->fmt & (FMT1 | FMT2 | FMT3); //處理fmt3 or fmt4 , fmt3:'1100' & '0111' = '0100' 得到 fmt3 
						}
						else if((line->fmt == FMT4) && ((op->fmt & FMT4) == 0)) /* INSTRUCTION is FMT1 or FMT 2*/
						{	
							printf("ERROR at token %s, %s cannot use format 4 \n", buf, buf);
							ret = LINE_ERROR;
							state = 7;		/* skip following tokens in the line */
						}
					}				
					else if(state == 0)	/* SYMBOL */
					{
						strcpy(line->symbol, buf);
						state = 1;
					}
					else				/* ERROR */
					{
						printf("ERROR at token %s\n", buf);
						ret = LINE_ERROR;
						state = 7;		/* skip following tokens in the line */
					}
					break;	
				case 3:
					if(line->fmt == FMT1 || line->code == 0x4C)	/* no operand needed *//*fmt1 or RSUB*/
					{
						if(c == EOF || buf[0] == '\n')
						{
							ret = LINE_CORRECT;
							state = 8;
						}
						else		/* COMMENT *//*後面若還有東西,則為註解*/
						{
							ret = LINE_CORRECT;
							state = 7;
						}
					}
					else  
					{
						if(c == EOF || buf[0] == '\n') /*抓到一個指令,不是fmt1 or RSUB,後面又沒有東西*/ 
						{
							ret = LINE_ERROR;
							state = 8;
						}
						else if(buf[0] == '@' || buf[0] == '#')  
						{
							line->addressing = (buf[0] == '#') ? ADDR_IMMEDIATE : ADDR_INDIRECT;
							state = 4;
						}
						else	/* get a symbol */
						{
							op = is_opcode(buf); /*確認不是保留字*/ 
							if(op != NULL)
							{
								printf("Operand1 cannot be a reserved word\n");
								ret = LINE_ERROR;
								state = 7; 		/* (D1093174) skip following tokens in the line */
							}
							else
							{
								strcpy(line->operand1, buf);
								state = 5;
							}
						}
					}			
					break;		
				case 4:
					op = is_opcode(buf);
					if(op != NULL)
					{
						printf("Operand1 cannot be a reserved word\n");
						ret = LINE_ERROR;
						state = 7;		/* skip following tokens in the line */
					}
					else
					{
						strcpy(line->operand1, buf);
						state = 5;
					}
					break;
				case 5:
					if(c == EOF || buf[0] == '\n')
					{
						ret = LINE_CORRECT;
						state = 8;
					}
					else if(buf[0] == ',')
					{
						state = 6;
					}
					else	/* COMMENT */
					{
						ret = LINE_CORRECT;
						state = 7;		/* skip following tokens in the line */
					}
					break;
				case 6:
					if(c == EOF || buf[0] == '\n')
					{
						ret = LINE_ERROR;
						state = 8;
					}
					else	/* get a symbol */
					{
						op = is_opcode(buf);
						if(op != NULL)
						{
							printf("Operand2 cannot be a reserved word\n");
							ret = LINE_ERROR;
							state = 7;		/* skip following tokens in the line */
						}
						else /*is a symbol*/ 
						{
							if(line->fmt == FMT2)
							{
								strcpy(line->operand2, buf);
								ret = LINE_CORRECT;
								state = 7;
							}
							else if((c == 1) && (buf[0] == 'x' || buf[0] == 'X'))
							{
								line->addressing = line->addressing | ADDR_INDEX;
								ret = LINE_CORRECT;
								state = 7;		/* D1093174 skip following tokens in the line */
							}
							else
							{
								printf("Operand2 exists only if format 2  is used\n");
								ret = LINE_ERROR;
								state = 7;		/* skip following tokens in the line */
							}
						}
					}
					break;
				case 7:	/* skip tokens until '\n' || EOF */
					if(c == EOF || buf[0] =='\n')
						state = 8;
					break;										
			}//end switch
			if(state < 8)
				c = ASM_token(buf);  /* get the next token */
		}
		return ret;
	}
}

//int main(int argc, char *argv[])
void pass1(char fname[])
{
	int		c;
	int		line_count;			
	char	temp[10];         /* 紀錄遇到 BYTE 時 "" 內的byte數 */
	


	{
		if(ASM_open(fname) == NULL)
			printf("File not found!!\n");
		else
		{
			for(line_count = 0 ; (c = process_line(&line[line_count])) != LINE_EOF; line_count++) //call process_line, 取一行出來, !=LINE_EOF會繼續迴圈 
			{
				
				/* 建symbol table */
				if(line[line_count].symbol[0] != '\0' )	
				{
					if(findsym(&line[line_count])==NOT_FOUND)
					{
						addsym(&line[line_count]);	
					}
				}
				
				/* 處理start address */
				if(strcmp(line[line_count].op, "START") == 0)
				{
					locctr = atoi(line[line_count].operand1);	     /* 用來計算address */
					start_locctr = atoi(line[line_count].operand1);  /* start_locctr = 起始位置 */
				} 
				
				/* LINE_ERROR\Comment line\FMT0\FMT1\FMT2\FMT3\FMT4 輸出*/
				if(c == LINE_ERROR)
				{
					printf("		  Error\n");
				}
				else if(c == LINE_COMMENT)
				{
					printf(" 	      Comment line\n");
					goto label;
				}	
				else if(line[line_count].fmt == FMT0)
				{
					if(!strcmp(line[line_count].op, "BYTE"))
					{
						line[line_count].address=locctr;
						char *t=line[line_count].operand1;
						switch(t[0])
            			{
                			case 'X':
                    			locctr += (strlen(t) - 2) / 2;
                    			break;
                			case 'C':
                    			locctr += strlen(t) - 3;
                    			break;
            			}
						//sscanf(line[line_count].operand1, "%*[^']'%[^']", temp );
						//line[line_count].address=locctr;
						//locctr += strlen(temp);
					}
					if(!strcmp(line[line_count].op, "WORD"))
					{
						line[line_count].address=locctr;
						locctr += 3;
					}
					if(!strcmp(line[line_count].op, "RESB"))
					{
						line[line_count].address=locctr;
						locctr += atoi(line[line_count].operand1)*1;
					}
					if(!strcmp(line[line_count].op, "RESW"))
					{
						line[line_count].address=locctr;
						locctr += atoi(line[line_count].operand1)*3;
					}
					if(!strcmp(line[line_count].op, "START"))
					{
						line[line_count].address=locctr;
					}
					if(!strcmp(line[line_count].op, "END"))
					{
						line[line_count].address=locctr;
					}
					if(!strcmp(line[line_count].op, "BASE"))
					{
						line[line_count].address=locctr;
						BASE=line[line_count].address;
						printf("%x\n", BASE);
					}		
				}/* end FMT0 */					
				else if(line[line_count].fmt == FMT1)
				{
					line[line_count].address=locctr;
					locctr += 1;	
				}
				else if(line[line_count].fmt == FMT2)
				{
					line[line_count].address=locctr;
					locctr += 2;	
				}
				else if(line[line_count].fmt == FMT3)
				{
					line[line_count].address=locctr;
					locctr += 3;		
				}
				else if(line[line_count].fmt == FMT4)
				{
					line[line_count].address=locctr;
					locctr += 4;	
				}
				//printf("%06X %12s %12s %12s,%12s (FMT=%X, ADDR=%X)\n", line[line_count].address, line[line_count].symbol, 
				//line[line_count].op, line[line_count].operand1, line[line_count].operand2, line[line_count].fmt, line[line_count].addressing);
				
				label:;	//meet comment line		
				
			}/*end for loop 處理完所有行*/ 
			
			numofline=line_count;
			
			programlength = locctr - start_locctr;
			//printf("\n\nProgram length = %X", locctr-start_locctr);
			//printf("\n\nSYMBOL TABLE:\n");
			//printsymbol();
			ASM_close();
		}
	}
}
