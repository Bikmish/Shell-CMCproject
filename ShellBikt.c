#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <readrs.h>
#include <arraysbuilder.h>
#include <gyspytricks.h>
struct cmd
{
	char** argv;
	char* fin;
	char* fout;
	char op[3];
	int pipe;
	int backgrnd;
	int new;
};
char* reader();						//читает и возвращает строку из stdin
void space_deleter(char*);			//удаляет повторяющиеся пробелы
void space_adder(char*);			//добавляет пробелы до и после операций
void shell(char***);
void brackets(char*);
int main(int argc, char** argv)
{
	char* str;
	char** wordarr;
	char*** argvs;
	int i = 0, j = 0;
	while(1 && !feof(stdin))
	{
		str = reader();
		//printf("Read line: %s\n", str);
		
		space_deleter(str);
		//printf("Deleting extra spaces: %s\n", str);
		
		brackets(str);
		space_deleter(str);
				
		wordarr = array_of_lines(str);
		
		argvs = argv_builder(wordarr);
		
		/*
		i=0;
		while(argvs[i])
		{
			while(argvs[i][j])
			{
				if(j==0)
					printf("Command %d: %s ",i,argvs[i][j]);
				else
					printf("%s ",argvs[i][j]);
				++j;
			}
			printf("\n");
			++i; j=0;
		}
		i=0;
		while(wordarr[i])
			printf("%s\n",wordarr[i++]);
		*/
		
		shell(argvs);
		
		//освобождение всей выделенной памяти
		for (i = 0; i < ARR_LENGTH; ++i) 
			free(argvs[i]);
		free(argvs);
		free(wordarr);
		free(str);
	}
}
char* reader()
{
	long curbufsz = BUF_SIZE, i=0;
	int c;
	char *str;
	if((str = (char*)malloc(curbufsz)) == NULL)
	{
		fprintf(stderr,"Malloc error!");
		exit(1);
	}
	while((c=getchar())!=EOF)
	{
		if(i>=curbufsz)
		{
			if((str = realloc(str,curbufsz+=BUF_SIZE))==NULL)
			{
				fprintf(stderr,"Realloc error!");
				exit(1);
			}
		}
		if(c=='\n')
			break;
		str[i++]=c;
	}
	str[i]=0;
	return str;
}
void space_deleter(char* str)
{
	space_adder(str);
	char flag = (str[0]==' ');
	long i=0,j=0;
	while(str[i]!=0)
	{
		if(str[i]==' ')
		{
			if(flag)
			{
				++i;
				continue;
			}
			flag = 1;
		}
		else
			flag = 0;
		str[j++]=str[i++];	
	}
	str[j]=0;
}
void space_adder(char* str)
{
	char* tempstr = (char*)malloc(strlen(str)*2);
	if(tempstr == NULL)
	{
		fprintf(stderr,"Malloc error!");
		exit(1);
	}
	long i=0,j=0;
	while(str[i]!=0)
	{
		if(str[i]=='|' && str[i+1]!='|') // ls|wc -> ls | wc
		{
			tempstr[j++]=' '; tempstr[j++]='|'; tempstr[j++]=' ';
			++i;
		}
		else if(str[i]=='|' && str[i+1]=='|') // ls||wc -> ls || wc
		{
			tempstr[j++]=' '; tempstr[j++]='|'; tempstr[j++]='|'; tempstr[j++]=' ';
			i+=2;
		}
		else if(str[i]=='&' && str[i+1]!='&') // ls&wc -> ls & wc
		{
			tempstr[j++]=' '; tempstr[j++]='&'; tempstr[j++]=' ';
			++i;
		}
		else if(str[i]=='&' && str[i+1]=='&') // ls&&wc -> ls && wc
		{
			tempstr[j++]=' '; tempstr[j++]='&'; tempstr[j++]='&'; tempstr[j++]=' ';
			i+=2;
		}
		else if(str[i]==';') // ls;wc -> ls ; wc
		{
			tempstr[j++]=' '; tempstr[j++]=';'; tempstr[j++]=' ';
			++i;
		}
		else if(str[i]=='>' && str[i+1]!='>') // ls>fout -> ls > fout
		{
			tempstr[j++]=' '; tempstr[j++]='>'; tempstr[j++]=' ';
			++i;
		}
		else if(str[i]=='<') // ls<fin -> ls < fin
		{
			tempstr[j++]=' '; tempstr[j++]='<'; tempstr[j++]=' ';
			++i;
		}
		else if(str[i]=='>' && str[i+1]=='>') // ls>>fout -> ls >> fout
		{
			tempstr[j++]=' '; tempstr[j++]='>'; tempstr[j++]='>'; tempstr[j++]=' ';
			i+=2;
		}
		else if(str[i]=='(' || str[i]==')')
		{
			tempstr[j++]=' ';
			++i;
		}
		else
			tempstr[j++]=str[i++];
	}
	tempstr[j]=0;
	
	strcpy(str,tempstr);
	free(tempstr);
}

void shell(char*** argvs)
{
	int i = 0, j = 0, fd[2], fd1[2], stin, curch = 0, pipecount = 0, exitst, fdnull;
	struct cmd curcmd, prevcmd;
	prevcmd.pipe = 0;
	stin = dup(0);

	pid_t pid;
	int fin, fout, status;
	while(argvs[i]!=NULL)
	{
		if(i>0)
			prevcmd = curcmd;

		curcmd.fout = curcmd.fin = NULL;
		curcmd.op[0]=curcmd.op[1]=curcmd.op[2]=0;
		curcmd.backgrnd = 0;
		curcmd.pipe = 0;
		curcmd.new = 1;
		curcmd.argv = argvs[i];
		
		while(argvs[i][j])
		{
			if(argvs[i][j][0]=='>')
			{
				curcmd.fout = argvs[i][j+1];
				if(argvs[i][j][1]=='>')
					curcmd.new = 0;
			}
			else if(argvs[i][j][0]=='<')
				curcmd.fin = argvs[i][j+1];
			else if(argvs[i][j][0]=='|') // | or ||
			{
				curcmd.op[0] = '|';
				if(argvs[i][j][1]=='|')
					curcmd.op[1] = '|';
				else
					curcmd.pipe = 1;
			}
			else if(argvs[i][j][0]=='&')
			{
				curcmd.op[0] = '&';
				if(argvs[i][j][1]=='&')
					curcmd.op[1] = '&';
				else
					curcmd.backgrnd = 1;
			}
			else if(argvs[i][j][0]==';')
			{
				curcmd.op[0] = ';';
			}
			++j;
		}
		j=0;
		while(argvs[i][j])
		{
			if(argvs[i][j][0]=='>' || argvs[i][j][0]=='<' || argvs[i][j][0]=='|' || argvs[i][j][0]==';' || argvs[i][j][0]=='&')
			{
				argvs[i][j]=NULL;
				break;
			}
			
			++j;
		}

		//////////// Конвейер////////////////////////
		if(curcmd.pipe)
			++pipecount;
		else
			pipecount = 0;
		
		if(pipecount%2 == 1)
		{
			if(pipe(fd) == -1) 
			{
				fprintf(stderr,"Pipe0 error!\n");
				exit(1);
			}
			if(pipe(fd1) == -1) 
			{
				fprintf(stderr,"Pipe1 error!\n");
				exit(1);
			}
		}
		/////////////////////////////////////////////
		
		
		
		//////////////////Обработка || и &&/////////////////////////
		if(i>0)
			if(prevcmd.op[0]=='|' && prevcmd.op[1]=='|' && exitst==0 || prevcmd.op[0]=='&' && prevcmd.op[1]=='&' && exitst==1)
			{
				++i; j=0;
				continue;
			}
		////////////////////////////////////////////////////////////
		
		////////////Фоновый режим////////////////
		if(curcmd.backgrnd)
		{
			if(!fork())
			{
				if(!fork())
				{
					if((fdnull=open("/dev/null",O_RDWR))==-1)
					{
						exit(1);
					}
					kill(getppid(),SIGUSR1);
					dup2(fdnull,0);
					//dup2(fdnull,1);
					signal(SIGINT,SIG_IGN);
					if(execvp(curcmd.argv[0],curcmd.argv)==-1)
						exit(1);
				}
				pause();
				exit(0);
			}
			else
			{
				wait(NULL);
				if(argvs[i+1]!=NULL)
				{
					++i; j=0;
					continue;
				}
				else
					return;
			}
		}
		
		
		if(!(pid = fork()))
		{
			//////////// Перенаправление////////////////////////////////////////////////
			if(curcmd.fin!=NULL && curcmd.backgrnd==0)
			{
				if((fin = open(curcmd.fin, O_RDONLY))==-1)
				{
						exit(1);
				}
				if(dup2(fin,0)==-1)
				{
						exit(1);
				}
			}
			if(curcmd.fout!=NULL && !curcmd.pipe && curcmd.backgrnd==0)
			{
				if(!curcmd.new) 	// >>
				{
					if((fout = open(curcmd.fout, O_CREAT|O_WRONLY|O_APPEND,0777))==-1)
					{
						exit(1);
					}
				}
				else			      	// >
					if((fout = open(curcmd.fout, O_CREAT|O_WRONLY|O_TRUNC,0777))==-1)
					{
						exit(1);	
					}
					
				if(dup2(fout,1)==-1)
						exit(1);
			}
			////////////////////////////////////////////////////////////////////////


			//////////// Конвейер////////////////////////////////////////
			if(curcmd.pipe && curch == 0) //если есть |, то пишем в канал
			{
				if(dup2(fd[1],1)==-1)
						exit(1);
				close(fd[1]);	
				close(fd[0]);
			}
			else if(curcmd.pipe && curch == 1)
			{
				if(dup2(fd1[1],1)==-1)
						exit(1);
				close(fd1[1]);	
				close(fd1[0]);
			}
			////////////////////////////////////////////////////////////
			
			
			if(execvp(curcmd.argv[0],curcmd.argv)==-1)
				{
					exit(1);	
				}
		}
		
		//////////// Обработка завершения сыновьего процесса ////////////
		if(!curcmd.backgrnd)
		{
			waitpid(pid,&status,0);
			if(WIFEXITED(status)!=0)
			{
				if((exitst = WEXITSTATUS(status))==1)
				{
						fprintf(stderr,"Wrong command!\n");
				}
			}
			else
			{
				exitst = 1;
			}
		}
		/////////////////////////////////////////////////////////////////
		
		if(curcmd.pipe && curch==0) //если была |, то следующая команда будет читать из канала
		{
			if(dup2(fd[0],0)==-1)
				exit(1);
			close(fd[0]);
			close(fd[1]);
		}
		else if(curcmd.pipe && curch==1)
		{
			if(dup2(fd1[0],0)==-1)
				exit(1);
			close(fd1[0]);
			close(fd1[1]);
		}
		else if(prevcmd.pipe==1 && curcmd.pipe==0)
		{
			dup2(stin,0);
			close(fd[0]);
			close(fd[1]);
			close(fd1[0]);
			close(fd1[1]);
		}
		
		//переключение канала
		if(curcmd.pipe)
			curch = (curch+1)%2;
		
		++i; j=0;
	}
}
void brackets(char* str)
{
	int i,j,k,fd[2],status,fout;
	pid_t pid;
	if(pipe(fd) == -1) 
	{
		fprintf(stderr,"Pipe error!\n");
		return;
	}	
	for(i=0; i<strlen(str); ++i)
	{
		if(str[i]=='(')
		{
			j=i;
			while(str[j]!=')' && str[j]!=0)
				++j;
			if(str[j]!=')')
			{
				fprintf(stderr,"Wrong command!\n");
				return;
			}
			
			write(fd[1],&str[i],j-i+1);
			if(!(pid=fork()))
			{
				if(str[j+2]=='>')
				{
					if((fout = open(&str[j+4], O_CREAT|O_WRONLY|O_TRUNC,0777))==-1)
					{
						exit(1);	
					}
					if(dup2(fout,1)==-1)
						exit(1);
				}
					
				if(str[j+3]=='>')
				{
					if((fout = open(&str[j+4], O_CREAT|O_WRONLY|O_APPEND,0777))==-1)
					{
						exit(1);
					}
					if(dup2(fout,1)==-1)
					exit(1);
				}
			}
			dup2(fd[0],0);
			close(fd[1]);
			close(fd[0]);
			if(execl("./Shell.out","./Shell.out",NULL)==-1)
				exit(1);
			fprintf(stderr,"Error!\n");
			return;
		}
		close(fd[0]);
			
		waitpid(pid,&status,0);
		if(WIFEXITED(status)!=0)
		{
			if((WEXITSTATUS(status))==1)
			{
					fprintf(stderr,"Wrong command!\n");
			}
		}
		for(k=i;k<=j;++k)
			str[k]=' ';
		
			
		if(str[j+2]=='&' || str[j+2]=='|' || str[j+2]==';')
			str[j+2]==' ';
		if(str[j+3]=='&' || str[j+3]=='|' || str[j+3]==';')
			str[j+3]==' ';
		i=j;
	}
		
}