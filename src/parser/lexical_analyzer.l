%option noyywrap
%{
/*****************声明和选项设置  begin*****************/
#include <stdio.h>
#include <stdlib.h>

#include "syntax_tree.h"
#include "syntax_analyzer.h"

int lines;
int pos_start;
int pos_end;

void pass_node(char *text){
     yylval.node = new_syntax_tree_node(text);
}

/*****************声明和选项设置  end*****************/

%}


%%
 /* to do for students */
 /* two cases for you, pass_node will send flex's token to bison */
\/\*([^\*]|(\*)*[^\*/])*(\*)*\*\/	{pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return NOTE;}
[ |\n] {pos_start = pos_end; pos_end += 1;}
[\t] {pos_start = pos_end; pos_end += 4;}
\+ 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return ADD;}
\- 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return SUB;}
\*  	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return MUL;}
\/	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return DIV;}
\< 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return LES;}
\<\=  	{pos_start = pos_end; pos_end += 2; pass_node(yytext); return LESE;}
\>	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return GRE;}
\>\= 	{pos_start = pos_end; pos_end += 2; pass_node(yytext); return GREE;}
\=\=  	{pos_start = pos_end; pos_end += 2; pass_node(yytext); return EQUAL;}
\!\=	{pos_start = pos_end; pos_end += 2; pass_node(yytext); return NEQUAL;}
\=	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return ASSIN;}
\; 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return SEMICOLON;}
\,  	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return COMMA;}
\(	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return LPAR;}
\) 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return RPAR;}
\[  	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return LSBRA;}
\]	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return RSBRA;}
\{   {pos_start = pos_end; pos_end += 1; pass_node(yytext); return LCBRA;}
\}   {pos_start = pos_end; pos_end += 1; pass_node(yytext); return RCBRA;}


else  	{pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return ELSE;}
if  	{pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return IF;}
int  	{pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return INT;}
return  	{pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return RETURN;}
void  	{pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return VOID;}
while  	{pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return WHILE;}
float  	{pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return FLOAT;}

[a-zA-Z]+  	{pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return ID;}
[0-9]+.|[0-9]*.[0-9]+ 	{pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return FLO;}
[0-9]+	{pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return INTEGER;}


. { pos_start = pos_end; pos_end++; return ERROR; }
\/\* 	{pos_start = pos_end; pos_end += 2; pass_node(yytext); return LNOTE;} 
\*\/  	{pos_start = pos_end; pos_end += 2; pass_node(yytext); return RNOTE;}
[a-zA-Z]	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return LETTER;}
[0-9] 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return DIG;}
 /****请在此补全所有flex的模式与动作  end******/
%%
