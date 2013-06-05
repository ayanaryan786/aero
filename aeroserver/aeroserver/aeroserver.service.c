#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "aero.common.h"
#include "aero.shell.h"
#include "aeroserver.common.h"
#include "aeroserver.db.h"
#include "aeroserver.cidades.h"
#include "aeroserver.voos.h"
#include "aeroserver.users.h"
#include "aeroserver.clients.h"

#define SOFICHEIRO "db.bin"

pDatabase db;
int server;

int doJob(pDatabase db, pRequest req, char *pipe);
void shutdownClients(pDatabase db);

void initRequest(pRequest r)
{
    r->pid = 0;
    r->isAdmin = 0;
    strcpy(r->command,"");
}

void initAction(pAction a)
{
    a->idAction = 0;
    strcpy(a->text,"");
}

void stopServer(int sinal)
{
    fflush(stdout);
    //Terminar Clientes ligados ao Servidor
    shutdownClients(db);
    //pipe do Servidor
    close(server);
    //Remover named pipe do Servidor
    unlink(SERVER);
    //Gravar DB
    saveDB(SOFICHEIRO,db);
    //Libertar memória
    freeDB(db);
    //Fechar a aplicação
    exit(0);
}

int startServer(int modeBG)
{
    Request req;
    char pipeClient[15];
    int shutDown = 0, res;
    char *dbName;
    
    //Verificar se o servidor já está a correr
    if (access(SERVER,W_OK) != -1)
    {
        printf("O servidor já está em execução\n");
        return 1;
    }
    //Verificar o caminho da base de dados
    dbName = getenv("SOFICHEIRO");
    if (!dbName)
    {
        printf("Configuração em falta: SOFICHEIRO\n");
    }
    //Verificar data atual
    if (!getenv("SODATA"))
    {
        printf("Configuração em falta: SODATA\n");
    }
    
    //Carregar dados do sistema
    db = loadDB(SOFICHEIRO);
    db->inBackground = modeBG;
    if (!db) return 1;
    
    //Modo Foreground: informação
    if (!db->inBackground)
    {
        printf("\nCidades:\n");
        showCidades(db->cidades);
        printf("\nVoos disponíveis:\n");
        showVoosDisponiveis(db->voos,0);
    }
    
    //Obter password do Administrador
    if (loadAdmin("SOADMPASS",db) != 0)
    {
        printf("Não foi possível obter Administrador: %s\n","SOADMPASS");
        return 1;
    }
    //Obter lista de utilizadores
    if (loadUsers("SOAGENTES",db) != 0)
    {
        printf("Não foi possível obter Utilizadores: %s\n","SOAGENTES");
        return 1;
    }
    
    saveData("SODATA",db);
    //Obter Data para o sistema
    if (loadData("SODATA",db) != 0)
    {
        printf("Não foi possível obter a data do sistema: %s\n","SODATA");
        return 1;
    }
    else if (db->data == 0)
    {
        printf("Não foi possível obter a data do sistema: %s\n","SODATA");
        return 1;
    }
    else
        printf("Data do Sistema: %d\n",db->data);
    
    return 1;
    
    //Sinal para parar o servidor
    signal(SIGUSR1,stopServer);
    //Criar pipe do Servidor
    mkfifo(SERVER,0644);
    
    server = open(SERVER,O_RDWR);
    do
    {
        initRequest(&req);
        //Ler pedidos
        read(server,&req,sizeof(Request));
        //Sem nada a tratar
        if (req.pid == 0)
            continue;
        //Recebido e preparar resposta
        if (req.isAdmin == 1)
            sprintf(pipeClient,"%s",ADMIN);
        else
            sprintf(pipeClient,"%d",req.pid);
        //Recebido e verificar comando
        if (strcmp(req.command,"") == 0)
        {
            if (!db->inBackground)
                printf("Sem comando: %s\n",pipeClient);
            continue;
        }
        else
            if (!db->inBackground)
                printf("(%s)comando recebido: %s\n",pipeClient,req.command);
        //Responder ao comando
        res = doJob(db,&req,pipeClient);
        if (res == SHUTDOWN) shutDown = 1;

    } while (shutDown == 0);
    stopServer(0);
    return 0;
}

int doJob(pDatabase db, pRequest req, char *pipe)
{
    int client, res = 0, auxBf;
    Action action;
    void *ptr;
    char *commandArgv[MAXCOMMANDARGS];
    int commandArgc = 0;
    pCidade auxCidadeOrigem, auxCidadeDestino;
    
    //Verificar acesso ao pipe do cliente
    if (access(pipe,W_OK) == -1)
    {
        printf("Sem acesso: %s\n",pipe);
        return NOACCESS;
    }
    
    //Inicializar dados de resposta
    initAction(&action);
    //Verificar credenciais
    action.idAction = checkLogin(db,req->username,req->password);
    
    //Abrir pipe do ciente
    client = open(pipe,O_WRONLY);
    //Mensagens em Foreground do estado
    if (!db->inBackground)
    {
        printf("(%s)%s fez pedido: %s\n",pipe,req->username,req->command);
        if (action.idAction == LOGIN_FAILED)
            printf("(%s)falhou login: %s\nabort\n",pipe,req->username);
    }
    
    //Interpretar os argumentos
    getCommandArgs(req->command,commandArgv,&commandArgc);
    
    //COMANDOS
    /**/ if (strcmp("login",commandArgv[0]) == 0)
    {
        if (action.idAction == LOGIN_OK)
        {
            if (!db->inBackground)
                printf("(%s)cliente adicionado: %s\n",pipe,req->username);
            addClientWithUser(db,req->pid,req->username);
        }
        sprintf(action.text,"%s",req->username);
    }
    else if (strcmp("logout",commandArgv[0]) == 0 && action.idAction == LOGIN_OK)
    {
        removeClient(db,req->pid);
    }
    else if (strcmp("shutdown",commandArgv[0]) == 0 && action.idAction == LOGIN_OK)
    {
        action.idAction = SUCCESS_REQ;
        sprintf(action.text,"%s","servidor a encerrar");
        res = SHUTDOWN;
    }
    else if (strcmp("info",commandArgv[0]) == 0 && action.idAction == LOGIN_OK)
    {
        action.idAction = SUCCESS_REQ;
        //Resposta
        if (db->clients)
        {
            strcpy(action.text,"Clientes activos:\n");
            ptr = db->clients;
            while (ptr)
            {
                snprintf(action.text,sizeof(action.text),"%s %d - %s\n",action.text,
                         ((pClient)ptr)->pid,getClientUsername((pClient)ptr));
                
                ptr = ((pClient)ptr)->next;
            }
        }
    }
    else if (strcmp("addcity",commandArgv[0]) == 0 && action.idAction == LOGIN_OK && commandArgc == 2)
    {
        action.idAction = SUCCESS_REQ;
        
        //ToDo - Rever
        if (db->totalCidades < MAXCITIES)
        {
            addCidade(db,commandArgv[1],0);
        }
    }
    else if (strcmp("addvoo",req->command) == 0 && action.idAction == LOGIN_OK && commandArgc == 5)
    {
        action.idAction = SUCCESS_REQ;
        //Cidades
        auxCidadeOrigem = findCidade(db->cidades,commandArgv[1]);
        if (!auxCidadeOrigem && !db->inBackground)
            printf("(%s)cidade origem \"%s\" não existe",pipe,commandArgv[1]);
        auxCidadeDestino = findCidade(db->cidades,commandArgv[2]);
        if (!auxCidadeDestino && !db->inBackground)
            printf("(%s)cidade destino \"%s\" não existe",pipe,commandArgv[2]);
        //Dia
        auxBf = atoi(commandArgv[3]);
        //Adiciona o voo
        addVoo(db,auxCidadeOrigem,auxCidadeDestino,auxBf);
    }
    else
    {
        action.idAction = NOEXIST_REQ;
        sprintf(action.text,"%s","não implementado");
        
        if (!db->inBackground)
            printf("(%s)não implementado\n",pipe);
    }
    //Enviar resposta
    write(client,&action.idAction,sizeof(int));
    write(client,action.text,sizeof(action.text));
    close(client);
    return res;
}

void shutdownClients(pDatabase db)
{
    pClient auxClient;
    auxClient = db->clients;
    while (auxClient)
    {
        kill(auxClient->pid,SIGUSR1);
        auxClient = auxClient->next;
    }
}