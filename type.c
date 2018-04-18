

// Type should always be Binary for this project. Not sending TYPE will
// default to text file retrieval. Required for RETR in this project.

// ORDER: Send TYPE, then send RETR. RETR uses settings of most recent TYPE command.

char* token;
char* filePath;

char* TYPE = "TYPE I";

char* RETR = "RETR ";

char[200] userFile;
scanf("%s", userFile);

//search for "get"keyword preceding file.
token = strtok(userFile, ' ');
filePath = strtok(NULL, '\0');
