#define SERVER "servidor"
#define ADMIN "administrador"
/*Valores máximos*/
#define MAXLOGIN 20
#define MAXCOMMAND 50
#define MAXCOMMANDARGS 5
#define MAXPIPE 15
#define MAXMESSAGE 255
#define MAXRETRIES 3
/*Outros*/
#define LOGIN_WAIT 5
#define MINPASSWORD 3
/*Actions*/
#define SUCCESS_REQ 1
#define NOEXIST_REQ 2
#define LOGIN_OK 3
#define LOGIN_FAILED 4
#define FAILED_REQ 5
#define SHUTDOWN 9
#define NOACCESS 10
#define PASSCHANGED 11
/*Mensagens*/
#define MSG_COMMANDSUCCESS "Comando executado com sucesso"
#define MSG_COMMANDFAILED "Ocorreram problemas"

typedef struct recRequest Request, *pRequest;
typedef struct recAction Action, *pAction;

struct recRequest {
    int pid;
    int isAdmin;
    char username[MAXLOGIN];
    char password[MAXLOGIN];
    char command[MAXCOMMAND];
};

struct recAction {
    int idAction;
    char message[MAXMESSAGE];
    /*Extended Text*/
    int hasText;
    int totalLines;
    char **textLines;
};