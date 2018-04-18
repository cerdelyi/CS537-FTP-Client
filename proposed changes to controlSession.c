
/*
 
 int controlSession(int controlSocket, int checkNumPaths, char* serverPath, char* localPath)

 char* CWD = "CWD \n"
//within "case 230:" add:



 if(checkNumPaths == 1)
    {
        dataSocket = dataConnSetup(originalResponse);
        char* CWD-DIR = malloc(strlen((CWD) +1) + strlen(serverPath));
        strcpy(CWD-DIR, CWD);
        strcat(CWD-DIR, serverPath);
        strcat(CWD-DIR, '\0');
        write(controlSocket, CWD-DIR, sizeof(CWD-DIR));
    }

*/
