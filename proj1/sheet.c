/******************************************************************************
 * sheet.c
 * @author: Martin Zmitko, xzmitk01
 * @description: 1. projekt IZP, jednoduchý terminálový program na úpravu 
 * tabulek. Jsou implementovány všechny příkazy kromě:
 * rseq, rsum, ravg, rmin, rmax, rcount, split, concatenate
 * @usage: 
 * ./sheet [-d DELIM] [Příkazy pro úpravu tabulky] 
 * ./sheet [-d DELIM] [Selekce řádků] [Příkaz pro zpracování dat]
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_ROW_SIZE 10242
#define MAX_COL_SIZE 101

#define EDIT_COUNT 8
#define DATA_COUNT 14
#define SELECTION_COUNT 3

#define EDIT_COMMAND_1_PARAM 1
#define EDIT_COMMAND_2_PARAM 2
#define EDIT_COMMAND_NO_PARAM 3

#define DATA_COMMAND_1_PARAM 4
#define DATA_COMMAND_2_PARAM 5
#define DATA_COMMAND_3_PARAM 7

#define NO_COMMAND 6

#define SELECTION_SATISFIED 1
#define SELECTION_UNSATISFIED 0
#define SELECTION_ERROR -1

typedef struct {
    char row[MAX_ROW_SIZE];
    int finalCols;
    int currentRow;
    int editType;
    char* delim;
} table_t;

typedef struct {
    int argc;
    char** argv;
} args_t;


////////////////////////////////////////////////////////////////////////////////
// Pomocné funkce
////////////////////////////////////////////////////////////////////////////////

// Funkce zkontroluje zda jsou zadány argumenty a uloží řetězec delimiteru.
int checkArgs(args_t args, table_t* table){
    if(args.argc == 1){
        table->delim = "";
        return EXIT_SUCCESS;
    }   
    //Nebyly zadány žádné argumenty a nemáme jak tabulku upravit.
    //Program se s chybovou hláškou ukončí.

    if(!strcmp(args.argv[1], "-d")) {
        if(args.argc <= 2){
            fprintf(stderr, "No delim or commands entered!\n");
            return EXIT_FAILURE;
        }
        table->delim = args.argv[2];
    }
    else {
        table->delim = " "; 
    }
    //kontrola, jestli je zadán argument delimiteru,
    //výsledek se uloží do struktury table.
    return EXIT_SUCCESS;
}

//Funkce vyhodnotí, jestli je načtený poslední řádek
bool isLastRow(){
    char c = getc(stdin);
    if(c == EOF){
        return true;
    }
    ungetc(c, stdin);
    return false;
}

//Funkce vrátí typ úprav, které se mají provádět a nastaví isSelector
//když je zadaný příkaz pro selekci
int getEditState(args_t args, int* commandPos, bool* isSelector){
    if(args.argc <= *commandPos){
        return NO_COMMAND;
    }

    const char* editCommands[EDIT_COUNT] = 
    {"irow", "icol", "drow", "dcol", "drows", "dcols", "arow", "acol"};

    for(int i = 0; i < EDIT_COUNT; i++){
        if(!strcmp(args.argv[*commandPos], editCommands[i])){
            if(i <= 3){
                return EDIT_COMMAND_1_PARAM;
            }
            else if(i == 4 || i == 5){
                return EDIT_COMMAND_2_PARAM;
            }
            else{
                return EDIT_COMMAND_NO_PARAM;
            }
        }
    }

    if(isSelector != NULL){
        *isSelector = false;
    } 
    const char* selectionCommands[SELECTION_COUNT] = 
    {"rows" , "beginswith", "contains"};

    for(int i = 0; i < SELECTION_COUNT; i++){
        if(!strcmp(args.argv[*commandPos], selectionCommands[i])){
            *commandPos += 3;
            if(isSelector != NULL){
                *isSelector = true;
            } 
            if(args.argc <= *commandPos){
                return NO_COMMAND;
            }
        }
    }

    const char* dataCommands[DATA_COUNT] = 
    {"cset", "tolower", "toupper", "round", "int", "copy", "swap", "move",
     "csum", "cavg", "cmin", "cmax", "ccount", "cseq"};

    for(int i = 0; i < DATA_COUNT; i++){
        if(!strcmp(args.argv[*commandPos], dataCommands[i])){
            if(i <= 4){
                return DATA_COMMAND_1_PARAM;
            }
            else if (i <= 7){
                return DATA_COMMAND_2_PARAM;
            }
            else{
                return DATA_COMMAND_3_PARAM;
            }
        }
    }

    fprintf(stderr, "Unknown command!\n");
    return SELECTION_ERROR;
}

// Funkce zjistí pozice začátku a konce daného sloupce col a 
// uloží je jako pole o dvou prvcích bounds
int getColBounds(table_t* table, int col, int* bounds){
    int curCol = 1;
    int curChar = 0;
    bounds[0] = 0;
    while(table->row[curChar] != 0){
        if(table->row[curChar] == table->delim[0]){
            if(curCol == col){
                bounds[1] = curChar;
                return EXIT_SUCCESS;
            }
            else{
                bounds[0] = curChar;
                curCol++;
            }
        }
        curChar++;
    }
    if(curCol == col){
        bounds[1] = curChar - 1;
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}

// Funkce najde v obsahu řádku všechny rozdělovací znaky a
// uloží počet řádků do struktury table.
int getNumOfCols(table_t* table){
    int cols = 1;
    for(int i = 0; table->row[i] != 0; i++){
        for(int j = 0; table->delim[j] != 0; j++){
            if(table->row[i] == table->delim[j]){
                table->row[i] = table->delim[0];
                cols++;
                break;
            }
        }
    }
    return cols;
}

// Funkce uloží do proměnné out čislo z následujícího argumentu po argPos a 
// vrátí 0. Pokud argument není číslo, není zadán nebo je menší než 1, vrátí 1
int get1Parameter(args_t args, int* argPos, int* out){
    if(*argPos == args.argc - 1){
        fprintf(stderr, "Invalid parameter!\n");
        return EXIT_FAILURE;
    }

    char* endPtr;
    *out = (int)strtol(args.argv[*argPos + 1], &endPtr, 10);
    if(*out < 1 || strcmp(endPtr, "")){
        fprintf(stderr, "Invalid parameter!\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// Funkce uloží do proměnných out1 a out2 čisla ze dvou následujících argumentů
// po argPos a vrátí 0.
// Pokud argument není číslo, není zadán nebo je menší než 1, vrátí 1
int get2Parameters(args_t args, int* argPos, bool restrictNM, bool printError, 
                   int* out1, int* out2){
    if(*argPos >= args.argc - 2){
        fprintf(stderr, "Invalid parameter!\n");
        return EXIT_FAILURE;
    }

    char *endPtr1, *endPtr2;
    *out1 = strtol(args.argv[*argPos + 1], &endPtr1, 10);
    *out2 = strtol(args.argv[*argPos + 2], &endPtr2, 10);
    if(*out1 < 1 || *out2 < 1 || (restrictNM && *out1 > *out2) 
    || strcmp(endPtr1, "") || strcmp(endPtr2, "")){
        if(printError){
            fprintf(stderr, "Invalid parameter!\n");
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// Funkce uloží do proměnných out1 a out2 a out3 čisla ze tří následujících
// argumentů po argPos a vrátí 0.
// Pokud argument není číslo nebo není zadán, vrátí 1
int get3Parameters(args_t args, int* argPos, int* out1, int* out2, int* out3){
    if(*argPos >= args.argc - 3){
        fprintf(stderr, "Invalid parameter!\n");
        return EXIT_FAILURE;
    }

    char *endPtr1, *endPtr2, *endPtr3;
    *out1 = strtol(args.argv[*argPos + 1], &endPtr1, 10);
    *out2 = strtol(args.argv[*argPos + 2], &endPtr2, 10);
    *out3 = strtol(args.argv[*argPos + 3], &endPtr3, 10);
    if(strcmp(endPtr1, "") || strcmp(endPtr2, "") || strcmp(endPtr3, "")){
        fprintf(stderr, "Invalid parameter!\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// Funkce zjistí počet sloupců po provedení příkazů úpravy tabulky
// (pro irow a arow)
void getFinalCols(table_t* table, args_t args, int currentCols){
    table->finalCols = currentCols;
    //projdeme všechny argumenty
    for(int i = 1; i < args.argc; i++){
        if(!strcmp(args.argv[i], "icol")){
            int arg;
            if(!get1Parameter(args, &i, &arg)){
                if(arg <= table->finalCols && arg > 0){
                    table->finalCols++;
                }
            } 
        }
        else if(!strcmp(args.argv[i], "acol")){
            table->finalCols++;
        }
        else if(!strcmp(args.argv[i], "dcol")){
            int arg;
            if(!get1Parameter(args, &i, &arg)){
                if(arg <= table->finalCols && arg > 0){
                    table->finalCols--;
                }
            }
        }
        else if(!strcmp(args.argv[i], "dcols")){
            int arg1, arg2;
            if(!get2Parameters(args, &i, true, false, &arg1, &arg2)){
                if(arg1 > 0 && arg2 >= arg1){
                    table->finalCols -= arg2 - arg1 + 1;
                }
            }
        }
        //tabulka vždy bude obsahovat alespoň 1 řádek
        if(table->finalCols <= 0){
            table->finalCols = 1;
        } 
    }
}

//Funkce zkontroluje, zda je nově načtený řádek validní
int checkNewRow(table_t* table, args_t args, int* currentCols){
    //Kontrola, jestli jsou na vstupu nějaká data. 
    //Jestli ne, program se s chybovým hlášením ukončí.
    if(strlen(table->row) <= 1){   
        fprintf(stderr, "Input table can't be empty!\n");
        return EXIT_FAILURE;
    }
    //kontrola, jestli není řádek větší, něž 10kiB. 
    //Jestli ano, program se s chybovým hlášením ukončí.
    if(strlen(table->row) == MAX_ROW_SIZE - 1 
    && table->row[MAX_ROW_SIZE - 1] != '\n'){
        fprintf(stderr, "Row %d is bigger than 10kiB!\n", table->currentRow);
        return EXIT_FAILURE;
    }   
    //Když se načítá první řádek, zjistíme počáteční a konečný počet řádků.
    if(*currentCols == -1){
        *currentCols = getNumOfCols(table);
        getFinalCols(table, args, *currentCols);
    } 
    else{
        int lastNumOfCols = *currentCols;
        int newCols = getNumOfCols(table);
        //Když se počet sloupců nerovná počtu v prvním řádku, ukončíme s chybou
        if(lastNumOfCols != newCols){
            fprintf(stderr, "Number of columns (%d) in row %d is not same"
                    " as the number of columns in the first row (%d)!\n", 
                    newCols, table->currentRow, *currentCols);
            return EXIT_FAILURE;
        }
    }

    //U každé buňky zkontrolujeme, jestli není delší než 100 znaků.
    for(int i = 1; i <= *currentCols; i++){
        int bounds[2];
        getColBounds(table, i, bounds);
        if(bounds[0] == 0){
            bounds[0] = -1;
        } 
        if(bounds[1] - bounds[0] >= MAX_COL_SIZE){
            fprintf(stderr,
            "Cell at row %d, column %d is bigger than 100 characters!\n",
            table->currentRow, i);
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

//funkce zapíše obsah buňky v řádku c do cellContent a vrátí 0, když takový
//řádek neexistuje, vrátí 1
int getCellContent(table_t* table, int C, char* cellContent){
    int bounds[2];
    if(getColBounds(table, C, bounds) == 0){
        //když vracíme obsah první buňky, začíná od 0. pozice, 
        //ostatní buňky začínají od pozice delimu + 1
        int offset = C == 1 ? 0 : 1; 
        memcpy(cellContent, &table->row[bounds[0] + offset],
        bounds[1] - bounds[0] - offset);
        cellContent[bounds[1] - bounds[0] - offset] = 0;
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////
// Funkce pro kontrolu selektoru
////////////////////////////////////////////////////////////////////////////////

//vyhodnocení jestli pro řádek vyhovuje selektor rows
//když ano, vrátí 1, když ne, vrátí 0, když nastala chyba, vrátí -1
int rows(table_t* table, args_t args, int selectorPos){
    int N, M;
    if(!get2Parameters(args, &selectorPos, true, false, &N, &M)){
        if(table->currentRow >= N && table->currentRow <= M){
            return SELECTION_SATISFIED;
        }
        else{
            return SELECTION_UNSATISFIED;
        } 
    }

    //kontrola nahrazení čísla -
    if(!strcmp(args.argv[selectorPos + 2], "-")){
        if(!strcmp(args.argv[selectorPos + 1], "-")){
            if(isLastRow()){
                return SELECTION_SATISFIED;
            }
            else{
                return SELECTION_UNSATISFIED;
            } 
        }
        else{
            if(get1Parameter(args, &selectorPos, &N)){
                return SELECTION_ERROR;
            }
            if(table->currentRow >= N){
                return SELECTION_SATISFIED;
            }
            else{
                return SELECTION_UNSATISFIED;
            } 
        }
    }
    fprintf(stderr, "Invalid parameter!\n");
    return SELECTION_ERROR;
}

//vyhodnocení jestli buňka na sloupci začína řetězcem str
//když ano, vrátí 1, když ne, vrátí 0, když nastala chyba, vrátí -1
int beginswith(table_t* table, args_t args, int selectorPos){
    int C;
    if(get1Parameter(args, &selectorPos, &C)){
        return SELECTION_ERROR;
    }

    char cellContent[MAX_COL_SIZE];
    if(getCellContent(table, C, cellContent)){
        return SELECTION_UNSATISFIED;
    }

    if(strlen(args.argv[selectorPos + 2]) > 100){
        fprintf(stderr, "Beginswith string exceeds 100 characters!\n");
        return SELECTION_ERROR;
    }

    if(!strncmp(cellContent, args.argv[selectorPos + 2],
                strlen(args.argv[selectorPos + 2]))){
        return SELECTION_SATISFIED;
    }
    else{
        return SELECTION_UNSATISFIED;
    } 
}

//vyhodnocení jestli buňka na sloupci obsahuje řetězec str
//když ano, vrátí 1, když ne, vrátí 0, když nastala chyba, vrátí -1
int contains(table_t* table, args_t args, int selectorPos){
    int C;
    if(get1Parameter(args, &selectorPos, &C)){
        return SELECTION_ERROR;
    }

    char cellContent[MAX_COL_SIZE];
    if(getCellContent(table, C, cellContent)){
        return SELECTION_UNSATISFIED;
    }

    if(strlen(args.argv[selectorPos + 2]) > 100){
        fprintf(stderr, "Contains string exceeds 100 characters!\n");
        return SELECTION_ERROR;
    }

    if(strstr(cellContent, args.argv[selectorPos + 2]) != NULL){
        return SELECTION_SATISFIED;
    }
    else{
        return SELECTION_UNSATISFIED;
    } 
}

//kontrola jestli selektor vyhovuje pro řádek
int checkSelector(table_t* table, args_t args, int selectorPos){
    if(args.argc <= selectorPos + 3){
        fprintf(stderr, "Invalid parameter!\n");
        return SELECTION_ERROR;
    }

    if(!strcmp(args.argv[selectorPos], "rows")){
        return rows(table, args, selectorPos);
    }
    else if(!strcmp(args.argv[selectorPos], "beginswith")){
        return beginswith(table, args, selectorPos);
    }
    else if(!strcmp(args.argv[selectorPos], "contains")){
        return contains(table, args, selectorPos);
    }
    
    return SELECTION_UNSATISFIED;
}

////////////////////////////////////////////////////////////////////////////////
// Funkce pro úpravu tabulky
////////////////////////////////////////////////////////////////////////////////

//Přidání řádku
int irow(table_t* table, int R){
    if(table->currentRow == R){
        for(int i = 0; i < table->finalCols - 1; i++){
            fprintf(stdout, "%c", table->delim[0]);
        }
        fprintf(stdout, "\n");
    }
    return EXIT_SUCCESS;
}

//Odstranění řádku
int drow(table_t* table, int R){
    if(table->currentRow == R){
        table->row[0] = '\0';
    }
    return EXIT_SUCCESS;
}

//Odstranění více řádků
int drows(table_t* table, int N, int M){
    if(table->currentRow >= N && table->currentRow <= M){
        table->row[0] = '\0';
    }
    return EXIT_SUCCESS;
}

//Přidání sloupce
int icol(table_t* table, int R){
    int bounds[2];
    if(getColBounds(table, R, bounds) == 0){
        if(strlen(table->row) > MAX_ROW_SIZE - 3){
            fprintf(stderr, "Row size increased over 10kib!\n");
            return EXIT_FAILURE;
        }

        for(int i = strlen(table->row) + 1; i > bounds[0]; i--){
            table->row[i] = table->row[i - 1];
        }
        table->row[bounds[0]] = table->delim[0];
    }

    return EXIT_SUCCESS;
}

//Odstranění sloupce
int dcol(table_t* table, int R){
    int bounds[2];
    if(getColBounds(table, R, bounds) == 0){
        int rowLen = strlen(table->row) + 1;
        int insPos = bounds[0];
        int i = bounds[1];
        if(R == 1){
            i++;
        } 
        for(; i < rowLen; i++){
            table->row[insPos] = table->row[i];
            insPos++;
        }
        //když se odstranili všechny řádky včetně \n, znova ho přidáme
        if(table->row[0] == 0){
            table->row[0] = '\n';
            table->row[1] = 0;
        }
    }

    return EXIT_SUCCESS;
}

//Odstranění více sloupců
int dcols(table_t* table, int N, int M){
    for(int i = N; i <= M; i++){
        dcol(table, N);
    }

    return EXIT_SUCCESS;
}

//Přidání sloupce na konec řádku
int acol(table_t* table){
    int rowLen = strlen(table->row);
    if(rowLen > MAX_ROW_SIZE -3){
        fprintf(stderr, "Row size increased over 10kib!\n");
        return EXIT_FAILURE;
    }

    if(strcmp(table->row, "")){
        table->row[rowLen - 1] = table->delim[0];
        table->row[rowLen] = '\n';
        table->row[rowLen + 1] = 0;
    }
    
    return EXIT_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
// Funkce pro úpravu dat
////////////////////////////////////////////////////////////////////////////////

//Změna buňky ve sloupci C na řetězec str
int cset(table_t* table, int C, char* str){
    int strLen = strlen(str);
    int rowLen = strlen(table->row);
    int bounds[2];
    if(getColBounds(table, C, bounds) == 0){
        int cellLen = bounds[1] - bounds[0];
        if(rowLen + strLen - cellLen >= MAX_ROW_SIZE){
            fprintf(stderr, "Row size increased over 10kib!\n");
            return EXIT_FAILURE;
        }
        if(strlen(str) > 100){
            fprintf(stderr, "Cset string exceeded 100 characters!\n");
            return EXIT_FAILURE;
        }

        char newRow[MAX_ROW_SIZE];
        int offset = C == 1 ? 0 : 1; 
        //kopírování všeho před nastavovanou buňkou
        memcpy(newRow, table->row, bounds[0] + offset);
        //kopírování obsahu nové buňky
        memcpy(&newRow[bounds[0] + offset], str, strLen);
        //kopírování zbytku řádku
        memcpy(&newRow[bounds[0] + strLen + offset], 
               &table->row[bounds[1]], rowLen - bounds[1] + 1);
        //kopírování nově vytvořeného řádku do řádku tabulky
        memcpy(table->row, newRow, MAX_ROW_SIZE);
    }
    return EXIT_SUCCESS;
}

//V buňce ve sloupci C převede všechna velká písmena na malá
int toLower(table_t* table, int C){
    int bounds[2];
    if(getColBounds(table, C, bounds) == 0){
        for(int i = bounds[0]; i < bounds[1]; i++){
            if(table->row[i] >= 'A' && table->row[i] <= 'Z'){
                table->row[i] += 32;
            }
        }
    }
    return EXIT_SUCCESS;
}

//V buňce ve sloupci C převede všechna malá písmena na velká
int toUpper(table_t* table, int C){
    int bounds[2];
    if(getColBounds(table, C, bounds) == 0){
        for(int i = bounds[0]; i < bounds[1]; i++){
            if(table->row[i] >= 'a' && table->row[i] <= 'z'){
                table->row[i] -= 32;
            }
        }
    }
    return EXIT_SUCCESS;
}

//Zaokrouhlí číslo ve sloupci C pokud obsahuje pouze platné číslo
int Round(table_t* table, int C, bool round){
    char cellContent[MAX_COL_SIZE];
    if(getCellContent(table, C, cellContent)){
        return EXIT_SUCCESS;
    }

    //převedem cell na číslo, pokud obsahuje něco jiného, ukončíme
    char* endPtr;
    double numInCell = strtod(cellContent, &endPtr);
    if(strcmp(endPtr, "")){
        return EXIT_SUCCESS;
    }

    //podle parametru funkce round zaokrouhlíme nebo odstraníme desetinné
    //místa
    char roundedCell[MAX_COL_SIZE];
    if(round){
        if(numInCell >= 0.0){
            sprintf(roundedCell, "%d", (int)(numInCell + 0.5));
        }
        else{
            sprintf(roundedCell, "%d", (int)(numInCell - 0.5));
        }
    }
    else{
        sprintf(roundedCell, "%d", (int)numInCell);
    }

    if(cset(table, C, roundedCell)){
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

//Přepíše obsah buněk ve sloupci M hodnotami ze sloupce N
int copy(table_t* table, int N, int M){
    char cellContent[MAX_COL_SIZE];
    if(getCellContent(table, N, cellContent)){
        return EXIT_SUCCESS;
    }

    if(cset(table, M, cellContent)){
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

//Výměna buněk N a M
int swap(table_t* table, int N, int M){
    char cellContentN[MAX_COL_SIZE], cellContentM[MAX_COL_SIZE];
    if(getCellContent(table, N, cellContentN) || 
       getCellContent(table, M, cellContentM)){
        return EXIT_SUCCESS;
    }

    if(cset(table, M, cellContentN) || cset(table, N, cellContentM)){
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

//Posunutí buňky N před sloupec M
int move(table_t* table, int N, int M){
    //při zadání stejných parametrů nic neděláme
    if(N == M){
        return EXIT_SUCCESS;
    }

    //sloupec N neexistuje 
    char cellContent[MAX_COL_SIZE];
    if(getCellContent(table, N, cellContent)){
        return EXIT_SUCCESS;
    }

    //sloupec M neexistuje
    int bounds[2];
    if(getColBounds(table, M, bounds)){
        return EXIT_SUCCESS;
    }

    dcol(table, N);
    //pokud jsme odstranili řádek před M, změnilo se číslo sloupce a musíme
    //ho zmenšít o 1
    if(M > N){
        M--;
    }

    if(icol(table, M) || cset(table, M, cellContent)){
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

//průměr nebo suma buněk M až N uložíme do C
int cavgsum(table_t* table, bool sumOrAvg, int C, int N, int M){
    //kontrola argumentů
    if(N < 1 || M < 1 || C < 1 || M < N || (C >= N && C <= M)){
        fprintf(stderr, "Invalid parameter!\n");
        return EXIT_FAILURE;
    }

    //z každého sloupce mezi N a M přičteme číslo k sum a 1 k numberOfNumbers
    double sum = 0;
    int numberOfNumbers = 0;
    for(int i = N; i <= M; i++){
        char cellContent[MAX_COL_SIZE];
        if(!getCellContent(table, i, cellContent)){
            char* endPtr;
            double numInCell = strtod(cellContent, &endPtr);
            if(strcmp(endPtr, "") || !strcmp(cellContent, "")){
                continue;
            }
            sum += numInCell;
            numberOfNumbers++;
        }
    }

    //V žádné z buňek nebylo číslo, vypíšeme NaN
    if(numberOfNumbers == 0){
        cset(table, C, "NaN");
        return EXIT_SUCCESS;
    }

    //když je sumOrAvg == true, vypočteme průměr, jinak necháme sumu
    if(sumOrAvg){
        sum /= numberOfNumbers;
    }
    
    //uděláme ze sum string
    char outContent[MAX_COL_SIZE];
    sprintf(outContent, "%f", sum);

    //zapíšeme do buňky C
    if(cset(table, C, outContent)){
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

//minimální nebo maximální (podle bool max) hodnotu z buněk N až M uložíme do C
int cminmax(table_t* table, bool maxOrMin, int C, int N, int M){
    //kontrola argumentů
    if(N < 1 || M < 1 || C < 1 || M < N || (C >= N && C <= M)){
        fprintf(stderr, "Invalid parameter!\n");
        return EXIT_FAILURE;
    }

    //v řádcích N až M najdeme nejmenší či největší číslo
    double out;
    bool outSet = false;
    for(int i = N; i <= M; i++){
        //uložíme obsah buňky, když neexistuje, pokračujeme na další
        char cellContent[MAX_COL_SIZE];
        if(getCellContent(table, i, cellContent)){
            continue;
        }

        //převedení obsahu buňky na double, když neobsahuje pouze číslo,
        //pokračujeme na další
        char* endPtr;
        double numInCell = strtod(cellContent, &endPtr);
        if(strcmp(endPtr, "") || !strcmp(cellContent, "")){
            continue;
        }

        if(!outSet){
            out = numInCell;
            outSet = true;
        }
        else{
            if(maxOrMin){
                if(numInCell > out){
                    out = numInCell;
                }
            }
            else{
                if(numInCell < out){
                    out = numInCell;
                }
            } 
        }
    }

    //Nenalezeno žádné číslo, vypíšeme NaN
    if(!outSet){
        cset(table, C, "NaN");
        return EXIT_SUCCESS;
    }

    //uděláme z výsledku string a uložíme ho do buňky C
    char outContent[MAX_COL_SIZE];
    sprintf(outContent, "%f", out);

    if(cset(table, C, outContent)){
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

//spočítáme neprázdné buňky ve sloupcích N až M, uložíme výsledek do C
int ccount(table_t* table, int C, int N, int M){
    //kontrola argumentů
    if(N < 1 || M < 1 || C < 1 || M < N || (C >= N && C <= M)){
        fprintf(stderr, "Invalid parameter!\n");
        return EXIT_FAILURE;
    }

    //z každého sloupce mezi N a M přičteme 1 k out když je sloupec neprázdný
    int out = 0;
    for(int i = N; i <= M; i++){
        char cellContent[MAX_COL_SIZE];
        if(!getCellContent(table, i, cellContent)){
            if(strcmp(cellContent, "")){
                out++;
            }
        }
    }

    //uděláme z výsledku string a nastavíme ho do buňky C

    char outContent[MAX_COL_SIZE];
    sprintf(outContent, "%d", out);

    if(cset(table, C, outContent)){
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

//do sloupců N až M uložíme postupně rostoucí čísla o 1 počínající od B
int cseq(table_t* table, int N, int M, int B){
    //kontrola argumentů
    if(N < 1 || M < 1 || M < N){
        fprintf(stderr, "Invalid parameter!\n");
        return EXIT_FAILURE;
    }

    for(int i = N; i <= M; i++){
        char numToCell[MAX_COL_SIZE];
        sprintf(numToCell, "%d", B);

        if(cset(table, i, numToCell)){
            return EXIT_FAILURE;
        }

        B++;
    }

    return EXIT_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
// Rozcestníky pro příkazy
////////////////////////////////////////////////////////////////////////////////

//načteme 1, 2 nebo 3 parametry, zjistíme pozici příštího případného příkazu,
//který uložíme do nextArgPos
int getDataEditArgs(args_t args, int* argPos,
                    int* N, int* M, int* C, int* nextArgPos){
    *nextArgPos = *argPos;
    switch(getEditState(args, argPos, NULL)){
        case DATA_COMMAND_1_PARAM:
            if(get1Parameter(args, argPos, N)) return EXIT_FAILURE;
            *nextArgPos += 2;
            break;
        case DATA_COMMAND_2_PARAM:
            if(get2Parameters(args, argPos, false, true, N, M)){
                return EXIT_FAILURE;
            } 
            *nextArgPos += 3;
            break;
        case DATA_COMMAND_3_PARAM:
            if(get3Parameters(args, argPos, N, M, C)){
                return EXIT_FAILURE;
            } 
            *nextArgPos += 4;
            break;
    }

    //v případě cset posuneme pozici pro čtení dalšího argumentu o 1
    //a zkontrolujeme, zda je zadaný parametr str
    if(strcmp(args.argv[*argPos], "cset") == 0){
        *nextArgPos += 1;
        if(args.argc <= *argPos + 2){
            fprintf(stderr, "Invalid parameter!\n");
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

//Načteme 1 nebo 2 parametry
int getTableEditArgs(args_t args, int* argPos, int* N, int* M){
    switch(getEditState(args, argPos, NULL)){
        case EDIT_COMMAND_1_PARAM:
            if(get1Parameter(args, argPos, N)){
                return EXIT_FAILURE;
            } 
            break;
        case EDIT_COMMAND_2_PARAM:
            if(get2Parameters(args, argPos, true, true, N, M)){
                return EXIT_FAILURE;
            } 
            break;
    }
    return EXIT_SUCCESS;
}

//Výběr příkazu pro úpravu tabulky
int doTableEdit(table_t* table, args_t args, int* argPos){
    int N, M;
    if(getTableEditArgs(args, argPos, &N, &M)){
        return EXIT_FAILURE;
    }

    if(strcmp(args.argv[*argPos], "irow") == 0){     
        *argPos += 2;
        return irow(table, N);
    }
    else if(strcmp(args.argv[*argPos], "drow") == 0){
        *argPos += 2;
        return drow(table, N);
    }
    else if(strcmp(args.argv[*argPos], "drows") == 0){
        *argPos += 3;
        return drows(table, N, M);
    }
    else if(strcmp(args.argv[*argPos], "icol") == 0){
        *argPos += 2;
        return icol(table, N);
    }
    else if(strcmp(args.argv[*argPos], "dcol") == 0){
        *argPos += 2;
        return dcol(table, N);
    }
    else if(strcmp(args.argv[*argPos], "dcols") == 0){
        *argPos += 3;
        return dcols(table, N, M);
    }
    else if(strcmp(args.argv[*argPos], "acol") == 0){
        *argPos += 1;
        return acol(table);
    }
    else if(strcmp(args.argv[*argPos], "arow") == 0){
        *argPos += 1;
        return EXIT_SUCCESS;
    }
    else{
        fprintf(stderr, "Unknown command or combination of"
                        " table and data editing commands!\n");
        return EXIT_FAILURE;
    }
}

//Výběr příkazu pro úpravu dat
int doDataEdit(table_t* table, args_t args, int* argPos){
    int N, M, C, nextArgPos;
    if(getDataEditArgs(args, argPos, &N, &M, &C, &nextArgPos)){
        return EXIT_FAILURE;
    }

    //více příkazů při úpravě dat není podporováno
    if(nextArgPos < args.argc){
        fprintf(stderr, "More than 1 data editing command not supported!\n");
        return EXIT_FAILURE;
    }

    if(strcmp(args.argv[*argPos], "cset") == 0){ 
        return cset(table, N, args.argv[*argPos + 2]);
    }
    else if(strcmp(args.argv[*argPos], "tolower") == 0){
        return toLower(table, N);
    }
    else if(strcmp(args.argv[*argPos], "toupper") == 0){
        return toUpper(table, N);
    }
    else if(strcmp(args.argv[*argPos], "round") == 0){
        return Round(table, N, true);
    }
    else if(strcmp(args.argv[*argPos], "int") == 0){
        return Round(table, N, false);
    }
    else if(strcmp(args.argv[*argPos], "copy") == 0){
        return copy(table, N, M);
    }
    else if(strcmp(args.argv[*argPos], "swap") == 0){
        return swap(table, N, M);
    }
    else if(strcmp(args.argv[*argPos], "move") == 0){
        return move(table, N, M);
    }
    else if(strcmp(args.argv[*argPos], "csum") == 0){
        return cavgsum(table, false, N, M, C);
    }
    else if(strcmp(args.argv[*argPos], "cavg") == 0){
        return cavgsum(table, true, N, M, C);
    }
    else if(strcmp(args.argv[*argPos], "cmin") == 0){
        return cminmax(table, false, N, M, C);
    }
    else if(strcmp(args.argv[*argPos], "cmax") == 0){
        return cminmax(table, true, N, M, C);
    }
    else if(strcmp(args.argv[*argPos], "ccount") == 0){
        return ccount(table, N, M, C);
    }
    else if(strcmp(args.argv[*argPos], "cseq") == 0){
        return cseq(table, N, M, C);
    }
    else{
        fprintf(stderr, "Unknown command or combination of"
                        " table and data editing commands!\n");
        return EXIT_FAILURE;
    }
}


int doCommands(table_t* table, args_t args, int firstArgPos, bool isSelector){
    int argPos = firstArgPos;
        
    //Příkaz pro úpravu tabulky
    if(table->editType == EDIT_COMMAND_1_PARAM ||
        table->editType == EDIT_COMMAND_2_PARAM ||
        table->editType == EDIT_COMMAND_NO_PARAM){
        while(argPos < args.argc){
            if(doTableEdit(table, args, &argPos)){
                return EXIT_FAILURE;
            }
        }
    }
    //Příkaz pro úpravu dat
    else if(table->editType == DATA_COMMAND_1_PARAM ||
            table->editType == DATA_COMMAND_2_PARAM ||
            table->editType == DATA_COMMAND_3_PARAM ||
            table->editType == NO_COMMAND){
        if(isSelector){
            int selectorResult = checkSelector(table, args, argPos - 3);
            //selektor je neplatný, ukončíme
            if(selectorResult == SELECTION_ERROR){
                return EXIT_FAILURE;
            }
            else if(selectorResult == SELECTION_SATISFIED){
                if(table->editType == NO_COMMAND){
                    return EXIT_SUCCESS;
                }
                if(doDataEdit(table, args, &argPos)){
                    return EXIT_FAILURE;
                }
            }
        }
        else{
            if(table->editType == NO_COMMAND){
                return EXIT_SUCCESS;
            }
            if(doDataEdit(table, args, &argPos)){
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// Vstupní bod programu
///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[]){
    args_t args = {argc, argv};
    table_t table;
    int currentCols = -1;
    table.currentRow = 1;

    //počáteční kontrola argumentů, uložení delimu
    if(checkArgs(args, &table)){
        return EXIT_FAILURE;
    }

    //pozice prvního příkazu podle zadání delimu
    int firstArgPos = strcmp(table.delim, " ") ? 3 : 1;
    bool isSelector;
    table.editType = getEditState(args, &firstArgPos, &isSelector);
    //neznámý příkaz, ukončíme program
    if(table.editType == SELECTION_ERROR){
        return EXIT_FAILURE;
    }

    //Hlavní smyčka programu, při každém průběhu se načítá řádek tabulky.
    while(fgets(table.row, MAX_ROW_SIZE, stdin)){ 
        if(checkNewRow(&table, args, &currentCols)){
            return EXIT_FAILURE;
        } 

        if(doCommands(&table, args, firstArgPos, isSelector)){
            return EXIT_FAILURE;
        }
        
        //vypíšeme zpracovaný řádek
        fprintf(stdout, "%s", table.row);
        table.currentRow++;
    }
    
    //pro poslední řádek zkontrolujeme a provedeme arow
    for(int i = 1; i < argc; i++){
        if(!strcmp(argv[i], "arow")){
            for(int i = 0; i < table.finalCols - 1; i++){
                fprintf(stdout, "%c", table.delim[0]);
            }
            fprintf(stdout, "\n");
        }
    }

    return EXIT_SUCCESS;
}