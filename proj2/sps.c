/******************************************************************************
 * sps.c
 * @author: Martin Zmitko, xzmitk01
 * @description: 2. IZP project, a simple terminal app for table editing using
 * dynamic memory. The table is read and writes to a file specified with 
 * launch arguments.
 * @usage:
 * ./sps [-d DELIM] 'command sequence' 'file' 
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define NOT_LAST_CELL 2
#define LAST_CELL 3
#define LAST_ROW 4

#define CELL(R, C) table->rows[R]->cells[C]->content

enum commands{UNKNOWN, SELECTION, SELECTION_MAX, SELECTION_MIN, SELECTION_FIND,
SELECTION_RESTORE, IROW, AROW, DROW, ICOL, ACOL, DCOL, SET_STR, CLEAR, SWAP,
SUM, AVG, COUNT, LEN, DEF_TEMP, USE_TEMP, INC_TEMP, SET_TEMP, GOTO, ISZERO, SUB};

typedef struct {
    int R1;
    int C1;
    int R2;
    int C2; 
} selection_t;

typedef struct {
    bool isNum;
    double num;
    char* text;
} tempVar_t;

typedef struct {
    char* content;
    int len;
    int allocLen;
} cell_t;

typedef struct {
    cell_t** cells;
    int len;
    int allocLen;
} row_t;

typedef struct {
    row_t** rows;
    int len;
    int allocLen;
    char* delim;
    selection_t selection;
    selection_t tmpSelection;
    tempVar_t** vars;
} table_t;

typedef struct {
    int argc;
    char** argv;
} args_t;

typedef struct {
    int name;
    char* str;
    int var;
    int var2;
    selection_t selection;
} command_t;

//free all memory used in table
void freeTable(table_t* table){
    for(int i = 0; i < table->len; i++){
        for(int j = 0; j < table->rows[i]->len; j++){
            free(table->rows[i]->cells[j]->content);
            free(table->rows[i]->cells[j]);
        }
        free(table->rows[i]->cells);
        free(table->rows[i]);
    }
    free(table->rows);
    free(table->delim);

    //10 temporary variables
    for(int i = 0; i < 10; i++){
        if(table->vars[i]->text != NULL) free(table->vars[i]->text);
        free(table->vars[i]);
    }
    free(table->vars);
}

//free all memory used by saved commands
void freeCmds(command_t* cmds, int cmdCount){
    for(int i = 0; i < cmdCount; i++){
        if(cmds[i].str != NULL) free(cmds[i].str);
    }
    free(cmds);
}

//checks if char is in delim
bool isDelim(char c, char* delim){
    int delimCount = strlen(delim);
    for(int i = 0; i < delimCount; i++){
        if(c == delim[i]){
            return true;
        }
    }
    return false;
}

//prints the table to file with correct formatting
void printTable(table_t* table, FILE* file){
    for(int i = 0; i < table->len; i++){
        for(int j = 0; j < table->rows[i]->len; j++){
            int len = strlen(CELL(i,j));
            bool printQuotes = false;
            for(int k = 0; k < len; k++){
                if(isDelim(CELL(i,j)[k], table->delim) || CELL(i,j)[k] == '"')
                    printQuotes = true;
            }
            if(printQuotes)
                fprintf(file, "\"");

            for(int k = 0; k < len; k++){
                if(CELL(i,j)[k] == '\\' || CELL(i,j)[k] == '"')
                    fprintf(file, "\\");
                fprintf(file, "%c", CELL(i,j)[k]);
            }
        
            if(printQuotes)
                fprintf(file, "\"");

            if(j != table->rows[i]->len - 1){
                fprintf(file, "%c", table->delim[0]);
            }
        }
        fprintf(file, "\n");
    }
} 

//Check the argument count and save correct delim, file name and commands strings
int getArgs(args_t args, char** delim, char** fileName, char** commands){
    if(args.argc < 3){
        fprintf(stderr, "Wrong number of arguments\n");
        return EXIT_FAILURE;
    }

    //Delim is set, there must be 5 arguments
    if(!strcmp(args.argv[1], "-d") && args.argc == 5){
        *delim = malloc(strlen(args.argv[2]) * sizeof(char) + 1);
        if(*delim == NULL){
            fprintf(stderr, "Argument memory allocation failed\n");
            return EXIT_FAILURE;
        }
        strcpy(*delim, args.argv[2]);
        *fileName = args.argv[4];
        *commands = args.argv[3];
        return EXIT_SUCCESS;
    }
    //delim isn't set, there must be 3 arguments
    else if(strcmp(args.argv[1], "-d") && args.argc == 3){
        *delim = malloc(sizeof(char) * 2);
        if(*delim == NULL){
            fprintf(stderr, "Argument memory allocation failed\n");
            return EXIT_FAILURE;
        }
        strcpy(*delim, " ");
        *fileName = args.argv[2];
        *commands = args.argv[1];
        return EXIT_SUCCESS;
    }
    //Not 3 or 5 arguments
    else{
        fprintf(stderr, "Wrong number of arguments\n");
        return EXIT_FAILURE;
    }
}

//check if delim has \ or "
bool checkDelim(char* delim){
    int len = strlen(delim);
    for(int i = 0; i < len; i++){
        if(delim[i] == '\\' || delim[i] == '"'){
            return true;
        }
    }
    return false;
}

row_t* row_ctor(){
    row_t* row = malloc(sizeof(row_t));
    if(row == NULL){
        return NULL;
    }

    row->len = 0;
    row->allocLen = 0;
    row->cells = NULL;

    return row;
}

cell_t* cell_ctor(){
    cell_t* cell = malloc(sizeof(cell_t));
    if(cell == NULL){
        return NULL;
    }

    cell->len = 0;
    cell->allocLen = 20;
    cell->content = calloc(cell->allocLen, sizeof(char));
    if(cell->content == NULL){
        free(cell);
        return NULL;
    }

    return cell;
}

tempVar_t* var_ctor(){
    tempVar_t* var = malloc(sizeof(tempVar_t));
    if(var == NULL){
        return NULL;
    }
    var->isNum = false;
    var->num = 1;
    var->text = calloc(1, sizeof(char));
    if(var->text == NULL) return NULL;
    return var;
}

tempVar_t** init_vars(){
    tempVar_t** vars = malloc(10 * sizeof(tempVar_t*));
    if(vars == NULL) return NULL;
    for(int i = 0; i < 10; i++){
        if((vars[i] = var_ctor()) == NULL){
            return NULL;
        }
    }
    return vars;
}

//appends a new row to the table
int add_row(table_t* table){
    row_t** newRows = table->rows;
    if(table->len + 1 >= table->allocLen){
        if(table->allocLen == 0) table->allocLen = 10;
        table->allocLen *= 2;
        newRows = realloc(table->rows, table->allocLen * sizeof(row_t*));
        if(newRows == NULL){
            return EXIT_FAILURE;
        }
    }
    
    newRows[table->len] = row_ctor();
    if(newRows[table->len] == NULL){
        free(newRows);
        return EXIT_FAILURE;
    }
    table->len++;
    table->rows = newRows;

    return EXIT_SUCCESS;
}

//appends a new cell to the table
int add_cell(row_t* row){
    cell_t** newCells = row->cells;
    if(row->len + 1 >= row->allocLen){
        if(row->allocLen == 0) row->allocLen = 10;
        row->allocLen *= 2;
        newCells = realloc(row->cells, row->allocLen * sizeof(cell_t*));
        if(newCells == NULL){
            return EXIT_FAILURE;
        }  
    }
    
    newCells[row->len] = cell_ctor();
    if(newCells[row->len] == NULL){
        free(newCells);
        return EXIT_FAILURE;
    }
    row->len++;
    row->cells = newCells;

    return EXIT_SUCCESS;
}

//inserts a new row before row R
int insert_row(table_t* table, int R){
    //selected row doesn't exist, append rows
    if(R >= table->len){
        while(table->len <= R){
            if(add_row(table)){
                fprintf(stderr, "Memory allocation error\n");
                return EXIT_FAILURE;
            }
        }
        return EXIT_SUCCESS;
    }

    //if allocated memory is too small, reallocate double the size
    row_t** newRows = table->rows;
    if(table->len + 1 >= table->allocLen){
        if(table->allocLen == 0) table->allocLen = 10;
        table->allocLen *= 2;
        newRows = realloc(table->rows, table->allocLen * sizeof(row_t*));
        if(newRows == NULL){
            fprintf(stderr, "Memory allocation error\n");
            return EXIT_FAILURE;
        }
    }
    
    //move rows after position R by one
    for(int i = table->len; i > R; i--){
        newRows[i] = newRows[i-1];
    }

    //create the row and save it
    newRows[R] = row_ctor();
    if(newRows[R] == NULL){
        free(newRows);
        fprintf(stderr, "Memory allocation error\n");
        return EXIT_FAILURE;
    }
    table->len++;
    table->rows = newRows;

    return EXIT_SUCCESS;
}

//insert a collumn before collumn C
int insert_col(table_t* table, int C){
    //for each row
    for(int i = 0; i < table->len; i++){
        //selected collumn doesn't exist, append new ones
        if(C > table->rows[i]->len){
            while(table->rows[i]->len <= C){
                if(add_cell(table->rows[i])){
                    fprintf(stderr, "Memory allocation error\n");
                    return EXIT_FAILURE;
                }
            }
            return EXIT_SUCCESS;
        }

        //double allocated memory if needed
        cell_t** newCols = table->rows[i]->cells;
        if(table->rows[i]->len + 1 >= table->rows[i]->allocLen){
            if(table->rows[i]->allocLen == 0) table->rows[i]->allocLen = 10;
            table->rows[i]->allocLen *= 2;
            newCols = realloc(table->rows[i]->cells, table->rows[i]->allocLen * sizeof(cell_t*));
            if(newCols == NULL){
                fprintf(stderr, "Memory allocation error\n");
                return EXIT_FAILURE;
            }
        }

        //move collumns after C by one
        for(int j = table->rows[i]->len; j > C; j--){
            newCols[j] = newCols[j - 1];
        }

        //create and save new empty collumn
        if((newCols[C] = cell_ctor()) == NULL){
            fprintf(stderr, "Memory allocation error\n");
            return EXIT_FAILURE;
        }

        table->rows[i]->len++;
        table->rows[i]->cells = newCols;
    }
    return EXIT_SUCCESS;
}

//Append empty cells to make all rows have the same number of collumns
int balanceTable(table_t* table){
    //find longest row
    int max = 0;
    for(int i = 0; i < table->len; i++){
        if(table->rows[i]->len > max){
            max = table->rows[i]->len;
        }
    }

    //append cells
    for(int i = 0; i < table->len; i++){
        for(int j = table->rows[i]->len; j < max; j++){
            if(add_cell(table->rows[i])){
                return EXIT_FAILURE;
            }
        }
    }

    return EXIT_SUCCESS;
}

//delete all collumns between positions C1 and C2
int dcol(table_t* table, int C1, int C2){
    for(int i = 0; i < table->len; i++){
        //delete collummns up to C2 or end of row, whichever is smaller
        int maxCol = C2 > table->rows[i]->len ? table->rows[i]->len : C2;
        for(int j = C1; j <= maxCol; j++){
            //free the memory
            free(table->rows[i]->cells[C1 - 1]->content);
            free(table->rows[i]->cells[C1 - 1]);

            //move rest of collumns by 1
            for(int k = C1; k < table->rows[i]->len; k++){
                table->rows[i]->cells[k - 1] = table->rows[i]->cells[k];
            }

            //reallocate smaller memory
            cell_t** newCols = realloc(table->rows[i]->cells, (table->rows[i]->len - 1) * sizeof(cell_t*));
            if(table->rows[i]->len - 1 != 0 && newCols == NULL){
                fprintf(stderr, "Memory allocation error\n");
                return EXIT_FAILURE;
            }
            table->rows[i]->cells = newCols;
            table->rows[i]->len--;
            table->rows[i]->allocLen--;
        }
    }
    return EXIT_SUCCESS;
}

//delete all rows between R1 and R2
int drow(table_t* table, int R1, int R2){
    //delete up to R2 or total number of rows, whichever is smaller
    int maxRow = R2 > table->len ? table->len : R2;
    for(int j = R1; j <= maxRow; j++){
        //free the memory
        for(int i = 0; i < table->rows[R1-1]->len; i++){
            free(table->rows[R1-1]->cells[i]->content);
            free(table->rows[R1-1]->cells[i]);
        }
        free(table->rows[R1-1]->cells);
        free(table->rows[R1-1]);

        //move rest of rows by 1
        for(int i = R1; i < table->len; i++){
            table->rows[i - 1] = table->rows[i];
        }

        //reallocate smaller memory
        row_t** newRows = realloc(table->rows, (table->len - 1) * sizeof(row_t*));
        if(table->len - 1 != 0 && newRows == NULL){
            fprintf(stderr, "Memory allocation error\n");
            return EXIT_FAILURE;
        }
        table->rows = newRows;
        table->len--;
        table->allocLen--;
    }
    
    return EXIT_SUCCESS;
}

//removes empty last collumns
int trimTable(table_t* table){
    int maxCol = 0;
    int maxRow = 0, maxRowNum = 0;
    //finds the longest row and its length
    for(int i = 0; i < table->len; i++){
        for(int j = 0; j < table->rows[i]->len; j++){
            if(strcmp(CELL(i, j), "")){
                if(j > maxCol){
                    maxCol = j;
                } 
            }
            if(j > maxRow){
                maxRow = j;
                maxRowNum = i;
            }
        }
    }

    if(dcol(table, maxCol + 2, table->rows[maxRowNum]->len)){
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

//expands the table up to cell [R,C]
int expandTable(table_t* table, int R, int C){
    while(table->len <= R){
        if(add_row(table)){
            fprintf(stderr, "Memory allocation failure\n");
            return EXIT_FAILURE;
        }
    }

    while(table->rows[R]->len <= C){
        if(add_cell(table->rows[R])){
            fprintf(stderr, "Memory allocation failure\n");
            return EXIT_FAILURE;
        }
    }

    if(balanceTable(table)){
        fprintf(stderr, "Memory allocation failure\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

//Appends one character to cell
int appendToCell(cell_t* cell, char c){
    char* newContent = cell->content;
    if(cell->len + 1 >= cell->allocLen){
        cell->allocLen *= 2;
        newContent = realloc(cell->content, cell->allocLen * sizeof(char));
        if(newContent == NULL){
            return EXIT_FAILURE;
        }
    }
    
    newContent[cell->len] = c;
    cell->content = newContent;
    cell->len++;
    return EXIT_SUCCESS;
}

//Reads cell content from file until delimiter or ending quote
int readCell(FILE* file, cell_t* cell, char* delim){
    char c = fgetc(file);
    //return correct value if cell is empty
    if(c == '\n') return LAST_CELL;
    else if(c == EOF) return LAST_ROW;
    else if(isDelim(c, delim)) return NOT_LAST_CELL;

    bool inQuotes = false;
    while(inQuotes || !(isDelim(c, delim) || c == '\n' || c == EOF)){
        if(c == '\\'){
            if((c = fgetc(file)) == '\n'){
                break;
            }
            fseek(file, -1, SEEK_CUR);
            //character is escaped, save next character
            if(appendToCell(cell, fgetc(file))){
                fprintf(stderr, "Memory allocation failure\n");
                return EXIT_FAILURE;
            }
        }
        else if(c == '"') inQuotes = !inQuotes;
        else{
            if(c == '\n'){
                fprintf(stderr, "No closing quote on row\n");
                return EXIT_FAILURE;
            }
            if(appendToCell(cell, c)){
                fprintf(stderr, "Memory allocation failure\n");
                return EXIT_FAILURE;
            }
        }
        c = fgetc(file);
    } 
    //append zero character to cell content to terminate it
    if(appendToCell(cell, 0)){
        fprintf(stderr, "Memory allocation failure\n");
        return EXIT_FAILURE;
    }
    //return correct value when cell isn't empty
    if(c == '\n' || fgetc(file) == EOF){
        if(fgetc(file) == EOF){
            return LAST_ROW;
        }
        fseek(file, -1, SEEK_CUR);
        return LAST_CELL;
    } 
    fseek(file, -1, SEEK_CUR);
    return NOT_LAST_CELL;
}

//Go through input file and read cell by cell
int readFile(FILE* file, char* delim, table_t* table){
    char c = fgetc(file);
    if(c == EOF){
        fprintf(stderr, "Input table empty\n");
        return EXIT_FAILURE;
    }

    //add first row and column
    if(add_row(table) || add_cell(table->rows[0])){
        fprintf(stderr, "Error while saving table to memory\n");
        return EXIT_FAILURE;
    }
    fseek(file, -1, SEEK_CUR);
    
    int currRow = 0, currCell = 0;

    while(true){
        int rslt = readCell(file, table->rows[currRow]->cells[currCell], delim);
        //there are more cells on row, increment currCell and continue
        if(rslt == NOT_LAST_CELL){
            if(add_cell(table->rows[currRow])){
                fprintf(stderr, "Error while saving table to memory\n");
                    return EXIT_FAILURE;
            }
            currCell++;
        }
        //last cell on row, increment currRow and reset currCell, continue
        else if(rslt == LAST_CELL){
            currRow++;
            currCell = 0;
            if(fgetc(file) == EOF) return EXIT_SUCCESS;
            else{
                fseek(file, -1, SEEK_CUR);
                if(add_row(table) || add_cell(table->rows[currRow])){
                    fprintf(stderr, "Error while saving table to memory\n");
                    return EXIT_FAILURE;
                }
            }
        }
        //last cell of table was read, exit function
        else if(rslt == LAST_ROW){
            return EXIT_SUCCESS;
        }
        else return EXIT_FAILURE;
    }
}

//parses and returns a text parameter for a command
char* parseStr(char* command){
    char* str = calloc(strlen(command) + 1, sizeof(char));
    if(str == NULL) return NULL;
    bool inQuotes = false;
    int offset = 0;
    int i = 0;
    while(command[i]){
        if(command[i] == '"') inQuotes = !inQuotes;
        else if(command[i] == '\\'){
            i++;
            str[offset] = command[i]; 
            offset++;
        }
        else{
            str[offset] = command[i];
            offset++;
        } 
        i++;
    }
    return str;
}

//parses and returns a selection command for a single cell command.name is UNKNOWN if
//selection failed, SELECTION if succeeded
command_t parseSingleSelect(char* command, table_t* table){
    command_t cmd;
    selection_t select;
    cmd.name = UNKNOWN;
    cmd.str = NULL;
    //convert row number, if replaced by _, save whole row
    char* endptr;
    int R = strtol(&command[1], &endptr, 10);
    if(R < 1){
        if(!strncmp(command, "[_,", strlen("[_,"))){
            select.R1 = 1;
            select.R2 = table->len;
            endptr = &command[2];
        }
        else return cmd;
    } 
    else{
        select.R1 = R;
        select.R2 = R;
    }
    //convert col number, if replaced by _, save whole col
    char* endptr2;
    R = strtol(&endptr[1], &endptr2, 10);
    if(R < 1){
        if(!strncmp(endptr2, "_]", strlen("_]"))){
            select.C1 = 1;
            select.C2 = table->rows[0]->len;
        }
        else return cmd;
    }
    else{
        select.C1 = R; 
        select.C2 = R;
    }
    
    cmd.name = SELECTION;
    cmd.selection = select;
    return cmd;
}

//parses and saves selection for multiple cells, command.name is UNKNOWN if
//selection failed, SELECTION if succeeded
command_t parseMultipleSelect(char* command, table_t* table){
    command_t cmd;
    selection_t select;
    cmd.name = UNKNOWN;
    cmd.str = NULL;
    //get first number
    char* endptr;
    int R = strtol(&command[1], &endptr, 10);
    if(R < 1){
        return cmd;
    }
    select.R1 = R;
    //get second number
    char* endptr2;
    R = strtol(&endptr[1], &endptr2, 10);
    if(R < 1){
        return cmd;
    }
    select.C1 = R;
    //get third number, can be -, then save max row
    R = strtol(&endptr2[1], &endptr, 10);
    if(R < 1){
        if(!strncmp(endptr2, ",-,", 3)){
            select.R2 = table->len;
            endptr = &endptr2[2];
        }
        else return cmd;
    }
    else select.R2 = R;
    //get fourth number, can be -, then save max collumn
    R = strtol(&endptr[1], &endptr2, 10);
    if(R < 1){
        if(!strncmp(endptr, ",-]", 3)){
            select.C2 = table->rows[0]->len;
        }
        else return cmd;
    }
    else select.C2 = R;

    //check if selection is valid
    if(select.R1 > select.R2 || select.C1 > select.C2) return cmd;
    cmd.name = SELECTION;
    cmd.selection = select;
    return cmd;
}

//parse selection commands
command_t parseSelection(char* command, table_t* table){
    command_t selector;
    selector.name = UNKNOWN;
    selector.str = NULL;
    //check for closing bracket
    char* closeBracket = strchr(command, ']');
    if(closeBracket == NULL || strcmp(closeBracket + 1, "")) return selector;
    //find command, save string argument
    if(!strncmp(command, "[find ", strlen("[find "))){
        char* newStr = calloc(strlen(&command[strlen("[find ")]), sizeof(char));
        if(newStr == NULL) return selector;
        strncpy(newStr, &command[strlen("[find ")], strlen(&command[strlen("[find ")]) - 1);
        selector.str = parseStr(newStr);
        free(newStr);
        if(selector.str == NULL) return selector;
        selector.name = SELECTION_FIND;
        return selector;
    }
    //count commas in command, parse single or multiple cell selection
    int commaCount = 0;
    for(char* i = command; i < closeBracket; i++){
        if(i[0] == ',') commaCount++;
    }
    switch(commaCount){
        case 1:
            selector = parseSingleSelect(command, table);
            break;
        case 3:
            selector = parseMultipleSelect(command, table);
            break;
    }
    return selector;
}

//parses and saves single cell arguments as selection
selection_t parseArg(char* command){
    selection_t selection = {0, 0, 0, 0};
    char* endptr;
    int R = strtol(command, &endptr, 10);
    if(R < 0) return selection;
    char* endptr2;
    int C = strtol(&endptr[1], &endptr2, 10);
    if(C < 0 || strcmp(endptr2, "]")) return selection;
    selection.R1 = R;
    selection.R2 = R;
    selection.C1 = C;
    selection.C2 = C;
    return selection;
}

//parses variable arguments
int parseVar(char* command){
    char* endptr;
    int X = strtol(command, &endptr, 10);
    if(X < 0 || X > 9) return -1;
    if(strcmp(endptr, "")) return -1;
    return X;
}

//parses and saves flow control commands
int parseControlVar(char* command, int type, int* var1, int* var2){
    char* endptr;
    if(type == ISZERO || type == SUB){
        if(command[0] != '_') return EXIT_FAILURE;
        *var1 = strtol(&command[1], &endptr, 10);
        if(*var1 < 0 || *var1 > 9) return EXIT_FAILURE;
        if(endptr[0] != ' ') return EXIT_FAILURE;
        endptr = &endptr[1];
    }
    else endptr = command;
    
    if(type == SUB){
        if(endptr[0] != '_') return EXIT_FAILURE;
        *var2 = strtol(&endptr[1], &endptr, 10);
        if(*var2 < 0 || *var2 > 9) return EXIT_FAILURE;
        if(strcmp(endptr, "")) return EXIT_FAILURE;
        return EXIT_SUCCESS;
    }

    bool positive;
    if(endptr[0] == '+') positive = true;
    else if(endptr[0] == '-') positive = false;
    else return EXIT_FAILURE;
    int X = strtol(&endptr[1], &endptr, 10);
    if(X < 0) return EXIT_FAILURE;
    if(strcmp(endptr, "")) return EXIT_FAILURE;

    if(!positive) X = -X;
    X--;
    if(type == GOTO) *var1 = X;
    else *var2 = X;
    return EXIT_SUCCESS;
}

//parses and saves commands with arguments
command_t parseCmdWithArg(char* command){
    command_t cmd;
    cmd.name = UNKNOWN;
    cmd.str = NULL;
    if(!strncmp(command, "set ", 4)){
        cmd.str = parseStr(&command[4]);
        if(cmd.str == NULL) return cmd;
        cmd.name = SET_STR;
    }
    else if(!strncmp(command, "swap [", 6)){
        cmd.selection = parseArg(&command[6]);
        if(cmd.selection.C1 == 0) return cmd;
        cmd.name = SWAP;
    }
    else if(!strncmp(command, "sum [", 5)){
        cmd.selection = parseArg(&command[5]);
        if(cmd.selection.C1 == 0) return cmd;
        cmd.name = SUM;
    }
    else if(!strncmp(command, "avg [", 5)){
        cmd.selection = parseArg(&command[5]);
        if(cmd.selection.C1 == 0) return cmd;
        cmd.name = AVG;
    }
    else if(!strncmp(command, "len [", 5)){
        cmd.selection = parseArg(&command[5]);
        if(cmd.selection.C1 == 0) return cmd;
        cmd.name = LEN;
    }
    else if(!strncmp(command, "count [", 7)){
        cmd.selection = parseArg(&command[7]);
        if(cmd.selection.C1 == 0) return cmd;
        cmd.name = COUNT;
    }
    else if(!strncmp(command, "def _", 5)){
        cmd.var = parseVar(&command[5]);
        if(cmd.var < 0) return cmd;
        cmd.name = DEF_TEMP;
    }
    else if(!strncmp(command, "use _", 5)){
        cmd.var = parseVar(&command[5]);
        if(cmd.var < 0) return cmd;
        cmd.name = USE_TEMP;
    }
    else if(!strncmp(command, "inc _", 5)){
        cmd.var = parseVar(&command[5]);
        if(cmd.var < 0) return cmd;
        cmd.name = INC_TEMP;
    }
    else if(!strncmp(command, "goto ", 5)){
        int var1, var2;
        if(parseControlVar(&command[5], GOTO, &var1, &var2))
            return cmd;
        cmd.var = var1;
        cmd.name = GOTO;
    }
    else if(!strncmp(command, "iszero ", 7)){
        int var1, var2;
        if(parseControlVar(&command[7], ISZERO, &var1, &var2))
            return cmd;
        cmd.var = var1;
        cmd.var2 = var2;
        cmd.name = ISZERO;
    }
    else if(!strncmp(command, "sub ", 4)){
        int var1, var2;
        if(parseControlVar(&command[4], SUB, &var1, &var2))
            return cmd;
        cmd.var = var1;
        cmd.var2 = var2;
        cmd.name = SUB;
    }

    return cmd;
}

//parses command, calls other functions to parse arguments, saves commands with no arguments
command_t parseCommand(char* command, table_t* table){
    command_t cmd;
    cmd.str = NULL;
    cmd.name = UNKNOWN;
    if(!strcmp(command, "irow")) cmd.name = IROW;
    else if(!strcmp(command, "icol")) cmd.name = ICOL;
    else if(!strcmp(command, "drow")) cmd.name = DROW;
    else if(!strcmp(command, "dcol")) cmd.name = DCOL;
    else if(!strcmp(command, "arow")) cmd.name = AROW;
    else if(!strcmp(command, "acol")) cmd.name = ACOL;
    else if(!strcmp(command, "[set]")) cmd.name = SET_TEMP;
    else if(!strcmp(command, "[min]")) cmd.name = SELECTION_MIN;
    else if(!strcmp(command, "[max]")) cmd.name = SELECTION_MAX;
    else if(!strcmp(command, "[_]")) cmd.name = SELECTION_RESTORE;
    else if(!strcmp(command, "clear")) cmd.name = CLEAR;
    else if(command[0] == '['){
        cmd = parseSelection(command, table);
    }
    else{
        cmd = parseCmdWithArg(command);
    }
    return cmd;
}

//parses all entered commands, returns them in array
command_t* parseCommands(char* commands, table_t* table, int* cmdCount){
    int count = 0;
    command_t* cmds = NULL;
    char* cmdStr = strtok(commands, ";");
    while(cmdStr != NULL){
        count++;
        command_t* newCmd = realloc(cmds, sizeof(command_t) * count);
        if(newCmd == NULL){
            if(commands != NULL) free(commands);
            fprintf(stderr, "Command memory allocation failed\n");
            return NULL;
        }
        cmds = newCmd;
        cmds[count - 1] = parseCommand(cmdStr, table);
        if(cmds[count - 1].name == UNKNOWN){
            fprintf(stderr, "Invalid command syntax\n");
            free(cmds);
            return NULL;
        }
        cmdStr = strtok(NULL, ";");
    }
    *cmdCount = count;
    return cmds;
}

//saves a number from cell on [R,C] to out, returns EXIT_SUCCESS if the cell
//contains only a number, EXIT_FAILURE if not
int getNumInCell(table_t* table, int R, int C, double* out){
    if(table->len <= R || table->rows[0]->len <= C) return EXIT_FAILURE;
    if(!strcmp(CELL(R, C), "")) return EXIT_FAILURE;
    char* endptr;
    *out = strtod(CELL(R, C), &endptr);
    if(strcmp(endptr, "")) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

//returns selection to first cell in current selection that contains command.str,
//returns selection to 0,0 if the string is not found in current selection
selection_t find(command_t command, table_t* table){
    for(int i = table->selection.R1; i <= table->selection.R2; i++){
        for(int j = table->selection.C1; j <= table->selection.C2; j++){
            if(strstr(CELL(i - 1, j - 1), command.str) != NULL){
                //string found
                selection_t out = {i, j, i, j};
                return out;
            }
        }
    }
    //string not found
    selection_t out = {0, 0, 0, 0};
    return out;
}

//returns selection to a cell with the min or max number, if no numbers are found,
//returns selection to 0,0
selection_t minMax(table_t* table, bool minOrMax){
    double num = minOrMax ? __DBL_MAX__ : __DBL_MIN__;
    selection_t selection = {0, 0, 0, 0};
    for(int i = table->selection.R1; i <= table->selection.R2; i++){
        for(int j = table->selection.C1; j <= table->selection.C2; j++){
            double tmp;
            if(!getNumInCell(table, i - 1, j - 1, &tmp)){
                if(minOrMax){
                    if(tmp < num){
                        num = tmp;
                        selection.R1 = i; selection.R2 = i;
                        selection.C1 = j; selection.C2 = j;
                    } 
                }
                else{
                    if(tmp > num){
                        num = tmp;
                        selection.R1 = i; selection.R2 = i;
                        selection.C1 = j; selection.C2 = j;
                    } 
                }
            }
        }
    }
    return selection;
}

//sets the content of cells in selection to str
int setStr(table_t* table, char* str, selection_t selection){
    for(int i = selection.R1 - 1; i < selection.R2; i++){
        for(int j = selection.C1 - 1; j < selection.C2; j++){
            if(expandTable(table, i, j)){
                fprintf(stderr, "Memory allocation failure\n");
                return EXIT_FAILURE;
            }
            int len = strlen(str);
            char* newContent = realloc(CELL(i, j), (len + 1) * sizeof(char));
            if(len != 0 && newContent == NULL){
                fprintf(stderr, "Memory allocation failure\n");
                return EXIT_FAILURE;
            }
            CELL(i, j) = newContent;
            strcpy(CELL(i, j), str);
            table->rows[i]->cells[j]->len = len + 1;
        }
    }
    return EXIT_SUCCESS;
}

//swaps all cells in table.selection with cell in selection
int swap(table_t* table, selection_t selection){
    cell_t* tmp;
    //save temporary shorter variables for readability
    int sR1 = selection.R1; int sC1 = selection.C1;
    int tR1 = table->selection.R1; int tR2 = table->selection.R2;
    int tC1 = table->selection.C1; int tC2 = table->selection.C2;

    //if selection isn't in table, expand it
    if(expandTable(table, sR1 > tR2 ? sR1 : tR2, sC1 > tC2 ? sC1 : tC2)){
        fprintf(stderr, "Memory allocation failure\n");
        return EXIT_FAILURE;
    }

    for(int i = tR1 - 1; i < tR2; i++){
        for(int j = tC1 - 1; j < tC2; j++){
            tmp = table->rows[i]->cells[j];
            table->rows[i]->cells[j] = table->rows[sR1 - 1]->cells[sC1 - 1];
            table->rows[sR1 - 1]->cells[sC1 - 1] = tmp;
        }
    }

    return EXIT_SUCCESS;
}

//saves a number as a string into cell in selection
int printNumToCell(table_t* table, selection_t selection, double num){
    //find the length of outputed number
    int bufsz = snprintf(NULL, 0, "%g", num);
    //allocate memory for temporary buffer
    char* outStr = malloc((bufsz + 1) * sizeof(char));
    if(outStr == NULL){
        fprintf(stderr, "Memory allocation failure\n");
        return EXIT_FAILURE;
    }
    //print in the buffer and put it in the selected cell
    sprintf(outStr, "%g", num);
    if(setStr(table, outStr, selection)){
        fprintf(stderr, "Memory allocation failure\n");
        free(outStr);
        return EXIT_FAILURE;
    }
    free(outStr);
    return EXIT_SUCCESS;
}

//gets the sum or average of numbers in table.selection and saves it in the
//cell from selection
int sumAvg(table_t* table, selection_t selection, bool isAvg){
    double sum = 0;
    int count = 0;
    for(int i = table->selection.R1 - 1; i < table->selection.R2; i++){
        for(int j = table->selection.C1 - 1; j < table->selection.C2; j++){
            double num;
            if(!getNumInCell(table, i, j, &num)){
                sum += num;
                count++;
            }
        }
    }
    if(isAvg) sum /= count;
    if(printNumToCell(table, selection, sum)){
        fprintf(stderr, "Memory allocation failure\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

//counts the number of non empty cells in table.selection, saves it in cell
//in selection
int count(table_t* table, selection_t selection){
    int count = 0;
    for(int i = table->selection.R1 - 1; i < table->selection.R2; i++){
        if(i >= table->len) break;
        for(int j = table->selection.C1 - 1; j < table->selection.C2; j++){
            if(j >= table->rows[i]->len) break;
            if(strcmp(CELL(i, j), "")) count++;
        }
    }

    if(printNumToCell(table, selection, count)){
        fprintf(stderr, "Memory allocation failure\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

//gets the length of all text in all cels in table.selection, saves it into
//cell in selection
int len(table_t* table, selection_t selection){
    int len = 0;

    for(int i = table->selection.R1 - 1; i < table->selection.R2; i++){
        for(int j = table->selection.C1 - 1; j < table->selection.C2; j++){  
            if(i < table->len && j < table->rows[i]->len)
                len += strlen(CELL(i, j));
        }
    }

    if(printNumToCell(table, selection, len)){
        fprintf(stderr, "Memory allocation failure\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

//saves the text or number in the first cell of table.selection in a temporary 
//variable var
int def(table_t* table, int var){
    double num;
    if(!getNumInCell(table, table->selection.R1 - 1, table->selection.C1 - 1, &num)){
        table->vars[var]->num = num;
        table->vars[var]->isNum = true;
    }
    else{
       char* newText = realloc(table->vars[var]->text, 
            strlen(CELL(table->selection.R1 - 1, table->selection.C1 - 1)) + 1 * sizeof(char));
        if(newText == NULL){
            fprintf(stderr, "Memory allocation failure\n");
            return EXIT_FAILURE;
        }
        strcpy(newText, CELL(table->selection.R1 - 1, table->selection.C1 - 1));
        table->vars[var]->text = newText;
        table->vars[var]->isNum = false;
    }
    return EXIT_SUCCESS;
}

//puts content of temporary variable var into all cells in table.selection
int use(table_t* table, int var){
    for(int i = table->selection.R1 - 1; i < table->selection.R2; i++){
        for(int j = table->selection.C1 - 1; j < table->selection.C2; j++){
            int err;
            if(table->vars[var]->isNum){
                err = printNumToCell(table, table->selection, table->vars[var]->num);
            }
            else{
                err = setStr(table, table->vars[var]->text, table->selection);
            }
            if(err){
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}

//increments the temporary variable var, if it isn't a number, set it to 1
void inc(table_t* table, int var){
    if(table->vars[var]->isNum) table->vars[var]->num++;
    else{
        table->vars[var]->isNum = true;
        table->vars[var]->num = 1;
    }
}

//do all the saved commands in sequence
int doCommands(table_t* table, command_t* commands, int cmdCount){
    for(int i = 0; i < cmdCount; i++){
        if(i < 0) return EXIT_FAILURE;
        switch(commands[i].name){
            selection_t select;
            case SELECTION:
                table->selection = commands[i].selection;
                break;
            case SELECTION_FIND:
                select = find(commands[i], table);
                if(select.C1 != 0) table->selection = select;
                break;
            case SELECTION_MAX:
                select = minMax(table, false);
                if(select.C1 != 0) table->selection = select;
                break;
            case SELECTION_MIN:
                select = minMax(table, true);
                if(select.C1 != 0) table->selection = select;
                break;
            case SET_TEMP:
                table->tmpSelection = table->selection;
                break;
            case SELECTION_RESTORE:
                table->selection = table->tmpSelection;
                break;
            case IROW:
                if(insert_row(table, table->selection.R1 - 1))
                    return EXIT_FAILURE;
                break;
            case AROW:
                if(insert_row(table, table->selection.R2))
                    return EXIT_FAILURE;
                break;
            case DROW:
                if(drow(table, table->selection.R1, table->selection.R2))
                    return EXIT_FAILURE;
                break;
            case DCOL:
                if(dcol(table, table->selection.C1, table->selection.C2))
                    return EXIT_FAILURE;
                break;
            case ICOL:
                if(insert_col(table, table->selection.C1 - 1))
                    return EXIT_FAILURE;
                break;
            case ACOL:
                if(insert_col(table, table->selection.C2))
                    return EXIT_FAILURE;
                break;
            case SET_STR:
                if(setStr(table, commands[i].str, table->selection))
                    return EXIT_FAILURE;
                break;
            case CLEAR:
                if(setStr(table, "", table->selection))
                    return EXIT_FAILURE;
                break;
            case SWAP:
                if(swap(table, commands[i].selection))
                    return EXIT_FAILURE;
                break;
            case SUM:
                if(sumAvg(table, commands[i].selection, false))
                    return EXIT_FAILURE;
                break;
            case AVG:
                if(sumAvg(table, commands[i].selection, true))
                    return EXIT_FAILURE;
                break;
            case COUNT:
                if(count(table, commands[i].selection))
                    return EXIT_FAILURE;
                break;
            case LEN:
                if(len(table, commands[i].selection))
                    return EXIT_FAILURE;
                break;
            case DEF_TEMP:
                if(def(table, commands[i].var))
                    return EXIT_FAILURE;
                break;
            case USE_TEMP:
                if(use(table, commands[i].var))
                    return EXIT_FAILURE;
                break;
            case INC_TEMP:
                inc(table, commands[i].var);
                break;
            case GOTO:
                i += commands[i].var;
                break;
            case ISZERO:
                if(table->vars[commands[i].var]->num == 0)
                    i += commands[i].var2;
                break;
            case SUB:
                table->vars[commands[i].var]->isNum = true;
                table->vars[commands[i].var2]->isNum = true;
                table->vars[commands[i].var]->num -= table->vars[commands[i].var2]->num;
                break;
        }
        //balance the table after each command so there are no missing collumns
        if(balanceTable(table)){
            fprintf(stderr, "Memory allocation failure\n");
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

//program entry point
int main(int argc, char** argv){
    //initial settings of structs
    args_t args = {argc, argv};
    table_t table;
    table.vars = init_vars();
    if(table.vars == NULL){
        fprintf(stderr, "Memory allocation failure\n");
        return EXIT_FAILURE;
    } 
    table.len = 0; table.rows = NULL;
    table.allocLen = 0;
    selection_t init = {1,1,1,1};
    table.selection = init;
    table.tmpSelection = init;
    char *fileName = NULL, *commands = NULL;
    //save and check arguments
    if(getArgs(args, &table.delim, &fileName, &commands)){
        return EXIT_FAILURE;
    }
    //check delim for quotes or backslashes
    if(checkDelim(table.delim)){
        fprintf(stderr, "Delim can't contain \\ or \"\n");
        freeTable(&table);
        return EXIT_FAILURE;
    }
    //open, read and save file contents into table
    FILE* file = fopen(fileName, "r");
    if(file == NULL){
        fprintf(stderr, "Error while reading file\n");
        freeTable(&table);
        return EXIT_FAILURE;
    }
    if(readFile(file, table.delim, &table) || balanceTable(&table)){
        fclose(file);
        freeTable(&table);
        return EXIT_FAILURE;
    }
    fclose(file);
    //read and save all commands
    int cmdCount;
    command_t* cmds = parseCommands(commands, &table, &cmdCount);
    if(cmds == NULL){
        freeTable(&table);
        return EXIT_FAILURE;
    }
    //execute all commands and trim excess collumns
    if(doCommands(&table, cmds, cmdCount) || trimTable(&table)){
        freeTable(&table);
        freeCmds(cmds, cmdCount);
        return EXIT_FAILURE;
    }
    //save the edited table in file
    file = fopen(fileName, "w");
    if(file == NULL){
        fprintf(stderr, "Error while reading file\n");
        freeTable(&table);
        freeCmds(cmds, cmdCount);
        return EXIT_FAILURE;
    }
    printTable(&table, file);
    //free everything and exit
    freeTable(&table);
    freeCmds(cmds, cmdCount);
    fclose(file);
    return EXIT_SUCCESS;
}