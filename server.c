#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define USER 0
#define ADMIN 2
#define UNAUTH_USER -1
#define RESPONSE_BYTES 512
#define REQUEST_BYTES 512
#define linesInMS 5
#define EXIT -1
//Struct used to save the User details in the Login details file
typedef struct login{
	char username[100];
	char password[100];
	char type;
	long long int accno;
	char active;
}user;
// Struct used to Save account number in the file
typedef struct seq{
    long long int count;
}acc_num;

void talkToClient(int client_fd);// To take username and password
void closeclient(int client_fd,char *str);// Terminate the connection
void userReqAdmin(char *username,char *password,int client_fd);//The user details seend by the admin

// Send Message to the Client in form of packets
// Here used the method of Packets because we don't have stuct to pass to the client for every thing and
// also there are uneven length message passed to the client  
void sendMsgtoClient(int clientFD, char *str) {
    int numPacketsToSend = (strlen(str)-1)/RESPONSE_BYTES + 1;
    int n = write(clientFD, &numPacketsToSend, sizeof(int));
    char *msgToSend = (char*)malloc(numPacketsToSend*RESPONSE_BYTES);
    strcpy(msgToSend, str);
    int i;
    for(i = 0; i < numPacketsToSend; ++i) {
        int n = write(clientFD, msgToSend, RESPONSE_BYTES);
        msgToSend += RESPONSE_BYTES;
    }
}
// Receive message from the client
char* recieveMsgFromClient(int clientFD) {
    int numPacketsToReceive = 0;
    int n = read(clientFD, &numPacketsToReceive, sizeof(int));
    if(n <= 0) {
        shutdown(clientFD, SHUT_WR);
        return NULL;
    }
    char *str = (char*)malloc(numPacketsToReceive*REQUEST_BYTES);
    memset(str, 0, numPacketsToReceive*REQUEST_BYTES);
    char *str_p = str;
    int i;
    for(i = 0; i < numPacketsToReceive; ++i) {
        int n = read(clientFD, str, REQUEST_BYTES);
        str = str+REQUEST_BYTES;
    }
    return str_p;
}

// convert long long int to str
char* strfromlonglong(long long int value){
  char buf[32], *p;
    long long int v;

  v = (value < 0) ? -value: value;
  p = buf + 31;
  do{
    *p -- = '0' + (v%10);
    v /= 10;
  } while(v);
  if(value < 0) *p -- = '-';
  p ++;

  int len = 32 - (p - buf);
  char *s = (char*)malloc(sizeof(char) * (len + 1));
  memcpy(s, p, len);
  s[len] = '\0';
  return s;
}

char* accountFromName(char *username){
	user usr;
	int fd = open("login.dat",O_RDONLY);
	while((read(fd,&usr,sizeof(usr)))>0){
		if(strcmp(usr.username,username)==0){
			char *buff = NULL;
			buff = strfromlonglong(usr.accno);
			//sprintf(buff,"%lld",usr.accno);
			return buff;
		}
	}
}

// mini statement from the file corresponding to the account number
char *printMiniStatement(char *db,int client_fd)
{
	//char *db = accountFromName(username);
	int fp = open(db, O_RDONLY);
	char *current_balance = (char *)malloc(20*sizeof(char));
	lseek(fp, 20, SEEK_CUR);
	char *miniStatement = (char *)malloc(1000*sizeof(char));
	read(fp, miniStatement, 1000);
	return miniStatement;
}

// get current Balance from the database
char *printBalance(char *db)
{
	int fd = open(db, O_RDONLY);
	char *current_balance = (char *)malloc(20*sizeof(char));
	read(fd, current_balance, 20);
	return current_balance;
}

// Update the file with the transaction details 
void updateTrans(char *db,char c,double balance,double amount)
{
	
	int fp = open(db, O_WRONLY | O_APPEND);
	int fpb = open(db, O_WRONLY);
	char *buff = (char *)malloc(20*sizeof(char));
	write(fpb, buff, 20);
	
	int length = sprintf(buff, "%20f",balance);
	lseek(fpb, 0, 0);
	write(fpb, buff, length);
	close(fpb);	
	
	char buffer[100];
	time_t ltime; /* calendar time */
	ltime=time(NULL); /* get current cal time */

	length = sprintf(buffer,"%.*s %f %c %f\n",(int)strlen(asctime(localtime(&ltime)))-1 , asctime(localtime(&ltime)), balance, c,amount);
	
	write(fp, buffer, length);
	close(fp);
	free(buff);
	

}

// Debit account
int Debit(char *username, int client_fd){

char usrupdate[] = "Transaction was successful.\n--------------------\n\nDo you want to continue the Transaction:: (no) to EXIT?";

	char *db = accountFromName(username);
	int fd = open(db, O_RDWR);

	struct flock fl;
	memset(&fl, 0, sizeof(fl));
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 20;
	// blocking mode F_SETLK it will not wait for the Lock it will return directly
	if(fcntl(fd, F_SETLK, &fl) == -1){
		sendMsgtoClient(client_fd, "\n\nAnother Transaction is Being Processed either Type back or exit to quit\n");
		close(fd);
		return 0;
		printf("cannot write lock\n");
		exit(1);
	}

	double balance=strtod(printBalance(db),NULL);
	double amount=0.0;
	sendMsgtoClient(client_fd, "Enter Amount");
	while(1){
		char *buff=recieveMsgFromClient(client_fd);
		amount=strtod(buff,NULL);
		if(amount<=0)
		sendMsgtoClient(client_fd,"Enter valid amount\nEnter Amount Again");
		else
		   break;
		}
		if(balance >= amount){
			balance -=amount;
			updateTrans(db,'D',balance,amount);
			sendMsgtoClient(client_fd,usrupdate);
		}
			
		else{
			
			return 2;					
		}
	
	fl.l_type = F_UNLCK;
	if(fcntl(fd, F_SETLK, &fl) == -1){
		printf("unlocked fail\n");
		exit(1);
	}
	
	close(fd);
	printf("User file updated\n");
}

// Credit account
int Credit(char *username, char *password, int client_fd){

    char usrupdate[] = "Transaction was successful.\n--------------------\n\nDo you want to continue the Transaction::(no) to EXIT?";

	// get account name from username
	char *db = accountFromName(username);
	int fd = open(db, O_RDWR);

	struct flock fl;
	memset(&fl, 0, sizeof(fl));
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 20;
	// blocking mode F_SETLK
	if(fcntl(fd, F_SETLK, &fl) == -1){
		printf("trying to open %s",db);
		sendMsgtoClient(client_fd, "\n\nAnother Transaction is Being Processed either Type back or exit to quit\n");
		
		close(fd);
		return 0;
		printf("cannot write lock\n");
		exit(1);
	}

	double balance=strtod(printBalance(db),NULL);
	double amount=0.0;
	sendMsgtoClient(client_fd, "Enter Amount to Add::");
	while(1){
		char *buff=recieveMsgFromClient(client_fd);
		amount=strtod(buff,NULL);
		if(amount<=0)
		sendMsgtoClient(client_fd,"Enter valid amount\nEnter Amount again ::");
		else
		   break;
		}
	balance +=amount;
	updateTrans(db,'C',balance,amount);
	sendMsgtoClient(client_fd,usrupdate);
	
	fl.l_type = F_UNLCK;
	if(fcntl(fd, F_SETLK, &fl) == -1){
		printf("unlocked fail\n");
		exit(1);
	}

	close(fd);
	printf("User File updated\n");

}
//Read the account number from the File ,increment it and again writeback to the File for the consistency
long long int getAccNo(){
	int fd = open("acc_num",O_RDWR);
	acc_num num;
    read(fd,&num,sizeof(num));
    long long int x = num.count+1;
    num.count = x;
    lseek(fd,0,SEEK_SET);
    write(fd,&num,sizeof(num));
	close(fd);
    return x;  
}

// check if username already exists
int checkUser(char *username)
{
	user usr;
	int fd = open("login.dat", O_RDONLY);
	while((read(fd, &usr, sizeof(usr))) > 0) {
		if(strcmp(usr.username,username)==0){
		
				close(fd);
				return 1;
			
        }
    }
    close(fd);
    return 0;
}

// Create a new account for the individual 
void AddUser(int client_fd){

	user usr;
	sendMsgtoClient(client_fd,"Enter username(UNIQUE)::");
	char *name = NULL;
	NAME:
	name = NULL;
	name = recieveMsgFromClient(client_fd);
	if(checkUser(name)){
		sendMsgtoClient(client_fd, "UserName already exists\nEnter username(UNIQUE)::");	
		goto NAME;		
	}
	
	sendMsgtoClient(client_fd, "Enter Password::");
	char *pass = NULL, *cpass = NULL;
	PASS:
	pass = recieveMsgFromClient(client_fd);
	sendMsgtoClient(client_fd, "ReEnter New Password::");
	cpass = recieveMsgFromClient(client_fd);
	if(strcmp(pass,cpass) == 0){ // passwords match, proceed with creating account
		printf("\npass verified");
		strcpy(usr.username,name);
		strcpy(usr.password,pass);
		usr.accno = getAccNo();
		usr.type = 'C';
		usr.active = 'y';
		printf("\nlogin.dat before");
		// create entry in login.dat
		int fd = open("login.dat", O_WRONLY | O_APPEND);
		write(fd, &usr, sizeof(usr));
		close(fd);
		printf("\nlogin.dat done");

		char *filename = NULL;
		filename = strfromlonglong(usr.accno);
		creat(filename, 0766);
		fd = open(filename, O_WRONLY | O_APPEND, 0766);
		double balance = 3000.000000000000000;
		char buff[20] = {0};
		sprintf(buff, "%f", balance);
		write(fd, buff, sizeof(buff));
		close(fd);
		printf("\n%s Created",filename);
	}
	else{
		sendMsgtoClient(client_fd, "Passwords do not match\nEnter New Password::");
		goto PASS;			
	}

}

// Create a joint account
void jointAcc(int client_fd){

	char *name1, *pass1, *name2, *pass2, *cpass;
	sendMsgtoClient(client_fd,"Enter 1st username(UNIQUE)::");
	name1 = NULL;
	NAME:				
	name1 = recieveMsgFromClient(client_fd);
	if(checkUser(name1)){
		sendMsgtoClient(client_fd, "UserName already exists\nEnter username(UNIQUE)::");	
		goto NAME;		
	}
	sendMsgtoClient(client_fd, "Enter Password::");
	PASS:
	pass1 = recieveMsgFromClient(client_fd);
	sendMsgtoClient(client_fd, "ReEnter New Password::");
	cpass = recieveMsgFromClient(client_fd);

	// 1st user creds verified
	if(strcmp(pass1,cpass) == 0){
		name2 = NULL;
		cpass = NULL;
		sendMsgtoClient(client_fd,"Enter 2nd username(UNIQUE)::");
		NAME2:				
		name2 = recieveMsgFromClient(client_fd);
		if(checkUser(name2)){
			sendMsgtoClient(client_fd, "UserName already exists\nEnter username(UNIQUE)::");	
			goto NAME2;		
		}
		sendMsgtoClient(client_fd, "Enter Password::");
		PASS2:
		pass2 = recieveMsgFromClient(client_fd);
		sendMsgtoClient(client_fd, "ReEnter New Password::");
		cpass = recieveMsgFromClient(client_fd);

		// user 2 password verified	
		if(strcmp(pass2,cpass)==0){
			// added both the users in login.dat	
			user usr1, usr2;
			long long int no = getAccNo();

			strcpy(usr1.username, name1);
			strcpy(usr1.password, pass1);
			usr1.type = 'J';
			usr1.active = 'y';
			usr1.accno = no;

			strcpy(usr2.username, name2);
			strcpy(usr2.password, pass2);
			usr2.type = 'J';
			usr2.active = 'y';
			usr2.accno = no;

			int fd = open("login.dat", O_WRONLY | O_APPEND);
			write(fd, &usr1,  sizeof(usr1));
			write(fd, &usr2,  sizeof(usr2));
			close(fd);
					
			// add mapping for usr1 and usr2
			// account name will be usr1's name. when usr2 will login, it'll access the respective usr1's file.

			char *filename = NULL;
			filename = strfromlonglong(no);
			creat(filename, 0766);
			fd = open(filename, O_WRONLY | O_APPEND, 0766);
			double balance = 3000.000000000000000;
			char buff[20] = {0};
			sprintf(buff, "%f", balance);
			write(fd, buff, sizeof(buff));
			close(fd);
			}
			// 2nd user's password not verified
			else{
				sendMsgtoClient(client_fd, "Passwords do not match\nEnter New Password::");
				goto PASS2;			
			}
		}

		// 1st user not verified
		else{
			sendMsgtoClient(client_fd, "Passwords do not match\nEnter New Password::");
			goto PASS;			
		}
}

// change the password
int Passwordchng(char *username, int client_fd){
char usrupdate[] = "Password updated successfully.\n--------------------\n\nDo you want to continue?";

	int fd = open("login.dat", O_RDWR);
	user usr;
	char *pass = NULL;
	char *cpass = NULL;
	while(read(fd, &usr, sizeof(usr)) > 0){
		if(strcmp(usr.username, username) == 0){
			sendMsgtoClient(client_fd, "Enter New Password::");
			ReEntry:
			pass = recieveMsgFromClient(client_fd);
			sendMsgtoClient(client_fd, "ReEnter New Password::");
			cpass = recieveMsgFromClient(client_fd);
			if(strcmp(pass,cpass) == 0){
				lseek(fd, -1*sizeof(user), SEEK_CUR);
				strcpy(usr.password, pass);
				write(fd, &usr, sizeof(usr));
				sendMsgtoClient(client_fd, usrupdate);
				return 1;		
			}
			else{
				sendMsgtoClient(client_fd, "Passwords do not match\nEnter New Password::");
				goto ReEntry;			
			}
		}
	
	}
	return 0;
}
// Deactivating the user if the admin want's to delete the user 
void deleteUser(int client_fd,char* name){
	
	// printf("Message received");
	
	int fd = open("login.dat",O_RDWR);
	user usr;
	
	while(read(fd,&usr,sizeof(usr))>0){
		if(strcmp(name,usr.username) == 0){
			
			printf("\ndeactivating USER");
			usr.active = 'n';
			lseek(fd, -1*sizeof(usr), SEEK_CUR);
			write(fd, &usr, sizeof(usr));
			break;
		}

	}


	close(fd);


}

// To manage the normal user if it logins either joint or normal user 
void userRequests(char *username,char *password,int client_fd)
{
	int flag=1;
	char option[] = "\n------------------\n\nEnter your choice\n1.Balance Enquiry\n2. Mini Statement(View Details)\n3. Deposit\n4. WithDraw\n5. Password Change\n6. User Details \nWrite exit for quitting.";
	sendMsgtoClient(client_fd,option);
	
	char *buff=NULL;
	while(flag)
	{
		if(flag == 100){
			flag = 1;
			sendMsgtoClient(client_fd,option);		
		}		
		
		if(buff!=NULL)
			buff=NULL;
		buff=recieveMsgFromClient(client_fd);

		int choice;

		if(strcmp(buff,"exit")==0)
			choice=7;
		else choice=atoi(buff);
		char *bal,*str;
		// printf("%d",choice);
		bal=(char *)malloc(1000*sizeof(char));
		str=(char *)malloc(100000*sizeof(char));
		strcpy(bal,"------------------\nAvailable Balance: ");
		strcpy(str,"------------------\nMini Statement: \n");
		char *db = accountFromName(username);
		char *acc_detail;
		acc_detail=(char *)malloc(1000*sizeof(char));
		

		char uname[50];
		strcpy(acc_detail,"\n------------------\nUsername::");
		strcat(acc_detail, username);
		strcat(acc_detail,"\nAccount Number:");
		strcat(acc_detail,db);

		char *error;
		error=(char *)malloc(1000*sizeof(char));
		switch(choice)
		{
			case 1:	
				strcat(bal,printBalance(db));
				sendMsgtoClient(client_fd,strcat(bal,option));
				free(bal);
				break;
			case 2:
				strcat(str, printMiniStatement(db,client_fd));
			 	sendMsgtoClient(client_fd,strcat(str,option));
				free(str);
				break;
			case 3:
				Credit(username, password, client_fd);
				buff=recieveMsgFromClient(client_fd);
				if(strcmp(buff,"back")==0)
					sendMsgtoClient(client_fd,option);	
				else if(strcmp(buff,"no")==0)
					flag = 0;
				else
					flag = 100;
				break;
			case 4:
				if((Debit(username, client_fd)) == 2)
				{
					strcpy(error,"\nInsufficient Balance\nPlease Try Again\n");
					sendMsgtoClient(client_fd,strcat(error,option));
				}
				else
				{
				buff=recieveMsgFromClient(client_fd);
				if(strcmp(buff,"back")==0)
					sendMsgtoClient(client_fd,option);
				else if(strcmp(buff,"no")==0)
					flag = 0;
				else
					flag = 100;
				}
				break;
			case 5:
				Passwordchng(username, client_fd);
				buff=recieveMsgFromClient(client_fd);
				if(strcmp(buff,"no")==0)
					flag = 0;
				else
					flag = 121;
				break;
			case 6:
				
				sendMsgtoClient(client_fd,strcat(acc_detail,option));
				
				// printf("\n Case 6 executed:");
				// userDetails(client_fd,username);
				break;
			case 7:
				flag=0;
				break;
			default:
				sendMsgtoClient(client_fd, "Unknown Query");
				break;
		}
	}
}



// Function to handle the Admin .
void adminRequests(char *username, int client_fd)
{
	char options[] = "\n1.Create Individual Account\n2.Create Joint Account\n3.Do changes in Customer Account\n4.Delete User\n5.Change Password\n6.User Details\nWrite exit to quit";
	sendMsgtoClient(client_fd, options);	
	char *dat = (char *)malloc(1000*sizeof(char));
	char *name = NULL;
	char *no=NULL;
	while(1){	
		char *buff=NULL;
		buff=recieveMsgFromClient(client_fd);	
		if(strcmp(buff,"exit")==0)
			break;
		else{	
			int choice=atoi(buff);
			switch(choice){
			// individual account
			case 1:
				AddUser(client_fd);	
				strcpy(dat, "\n----------------USER ADDED SUCCESSFULLY----------------\n");
				sendMsgtoClient(client_fd, strcat(dat,options));
				break;

			// joint account
			case 2:
				jointAcc(client_fd);
				strcpy(dat,"\n----------------Joint Account created successfully----------------\n");
				sendMsgtoClient(client_fd,strcat(dat,options));
				break;

			// get user details
			case 3:
				sendMsgtoClient(client_fd,"Enter username::");
				name = NULL;
				name = recieveMsgFromClient(client_fd);
				if(checkUser(name))
				{
				userReqAdmin(name,NULL,client_fd);
				}
				else
				{
					strcpy(dat,"\nUser not found\n");
					sendMsgtoClient(client_fd,strcat(dat,options));
				}
				break;

			// delete user
			case 4:
				sendMsgtoClient(client_fd,"Enter the Username::");
				char *buff=NULL;
				buff=recieveMsgFromClient(client_fd);
				deleteUser(client_fd,buff);
				strcpy(dat,"Account deleted\n");
				sendMsgtoClient(client_fd,strcat(dat,options));
				break;
			
			// change password
			case 5:
				Passwordchng(username, client_fd);
				break;
			//User Details
			case 6:
				sendMsgtoClient(client_fd,"Enter Username::");
				buff=NULL;
				buff = recieveMsgFromClient(client_fd);
				if(checkUser(buff))
				{
					user usr;
					strcat(dat,"\n------------------------------------");
					strcat(dat,"\nUsername:");
					strcat(dat,buff);
					no=accountFromName(buff);
					strcat(dat,"\nAccount Number:");
					strcat(dat,no);
					strcat(dat,"\nAccount Status:");
					int fd1;
					fd1=open("login.dat",O_RDONLY);
					while(read(fd1,&usr,sizeof(usr)) >0)
					{
						if(strcmp(usr.username,buff)==0)
						{
							if(usr.active == 'y')
							{
								strcat(dat,"Active");
							}
							else
								strcat(dat,"Inactive");
						}
					}
					strcat(dat,"\nBalance:");
					strcat(dat,printBalance(no));
					strcat(dat,"\n------------------------------------");
					sendMsgtoClient(client_fd,strcat(dat,options));
				}
				else
				{
					strcpy(dat,"\n User Doesn't exist");
					sendMsgtoClient(client_fd,strcat(dat,options));
				}
				break;
			default:
				strcpy(dat, "Invalid Choice"); 
				sendMsgtoClient(client_fd, strcat(dat,options));
				break;
			}	
		}
	}
}
// Function specifically for the Admin to do the changes to the User 
void userReqAdmin(char *username,char *password,int client_fd)
{
	int flag=1;
	char option[] = "\n------------------\n\nEnter your choice\n1. Balance Inquiry\n2. Mini Statement(View Details)\n3. Password Change\n4. User Details \n5.Go Back to Main menu\nWrite exit for quitting.";
	sendMsgtoClient(client_fd,option);
	
	char *buff=NULL;
	while(flag)
	{
		if(flag == 121){
			flag = 1;
			sendMsgtoClient(client_fd,option);		
		}		
		
		if(buff!=NULL)
			buff=NULL;
		buff=recieveMsgFromClient(client_fd);

		int choice;

		if(strcmp(buff,"exit")==0)
			choice=8;
		else choice=atoi(buff);
		char *bal,*str;
		// printf("%d",choice);
		bal=(char *)malloc(1000*sizeof(char));
		str=(char *)malloc(100000*sizeof(char));
		strcpy(bal,"------------------\nAvailable Balance: ");
		strcpy(str,"------------------\nMini Statement: \n");
		char *db = accountFromName(username);
		char *acc_detail;
		acc_detail=(char *)malloc(1000*sizeof(char));
		

		char uname[50];
		strcpy(acc_detail,"\n------------------\nUsername::");
		strcat(acc_detail, username);
		strcat(acc_detail,"\nAccount Number:");
		strcat(acc_detail,db);

		switch(choice)
		{
			case 1:	
				strcat(bal,printBalance(db));
				sendMsgtoClient(client_fd,strcat(bal,option));
				free(bal);
				break;
			case 2:
				strcat(str, printMiniStatement(db,client_fd));
			 	sendMsgtoClient(client_fd,strcat(str,option));
				free(str);
				break;
			case 50:
				Credit(username, password, client_fd);
				buff=recieveMsgFromClient(client_fd);
				if(strcmp(buff,"back")==0)
					sendMsgtoClient(client_fd,option);	
				else if(strcmp(buff,"no")==0)
					flag = 0;
				else
					flag = 121;
				break;
			case 40:
				Debit(username, client_fd);
				buff=recieveMsgFromClient(client_fd);
				if(strcmp(buff,"back")==0)
					sendMsgtoClient(client_fd,option);
				else if(strcmp(buff,"no")==0)
					flag = 0;
				else
					flag = 121;
				break;
			case 3:
				Passwordchng(username, client_fd);
				buff=recieveMsgFromClient(client_fd);
				if(strcmp(buff,"no")==0)
					flag = 0;
				else
					flag = 121;
				break;
			case 4:
				
				sendMsgtoClient(client_fd,strcat(acc_detail,option));
				
				// printf("\n Case 6 executed:");
				// userDetails(client_fd,username);
				break;
			case 5:
				adminRequests(NULL,client_fd);
				break;
			case 8:
				flag=0;
				break;
			default:
				sendMsgtoClient(client_fd, "Unknown Query");
				break;
		}
	}
}
// it is used to initialize the username and password from the received client to the server username and client
void getupcli(char *username,char *password,int client_fd)
{
	char *ruser,*rpass;
	sendMsgtoClient(client_fd,"Enter Username: ");
	ruser=recieveMsgFromClient(client_fd);

	sendMsgtoClient(client_fd,"Enter Password: ");
	rpass=recieveMsgFromClient(client_fd);

	int i=0;
	while(ruser[i]!='\0' && ruser[i]!='\n')
	{
		username[i]=ruser[i];
		i++;
	}

	username[i]='\0';

	i=0;
	while(rpass[i]!='\0' && rpass[i]!='\n')
	{
		password[i]=rpass[i];
		i++;
	}
	password[i]='\0';

}
// Checking that the username and password exists . If it does the check the status of the account it is active and allow login.
int authorize(char* username,char *password)
{
	printf("Authorizing\n");
    ssize_t readc;
	user usr;
	int fd = open("login.dat", O_RDONLY);
	while((readc = read(fd, &usr, sizeof(usr))) > 0){
		if(strcmp(usr.username,username)==0){
			if(strcmp(usr.password,password)==0){		
                if(usr.type=='C' || usr.type == 'J'){	
					// active account
					if(usr.active=='y'){
						printf("\naccepting");
						close(fd);
                    	return USER;
					}
					else
					{
						printf("\nrejecting");
						close(fd);
						return UNAUTH_USER;
					}
		    	}
                else if(usr.type =='A'){
                    close(fd);
                    return ADMIN;
                }
            }
        }
    }

    close(fd);
	return UNAUTH_USER;
}
// gets the username and password from the Client and Calls the respective function for admin and the user 
void talkToClient(int client_fd)
{
	char *username,*password;
	username=(char *)malloc(100);
	password=(char *)malloc(100);
	int utype;
	
	getupcli(username,password,client_fd); // get username and password from the user
	utype=authorize(username,password); // validate creds and account (whether or not it is active)

	char *str=(char *)malloc(sizeof(char)*60);
	strcpy(str,"Thanks ");

	switch(utype)
	{
		case USER:
			printf("User is IN \n");
			userRequests(username,password,client_fd);
			closeclient(client_fd,strcat(str,username));
			break;
		case ADMIN:
			printf("ADMIN is IN \n");
			adminRequests(username, client_fd);
			closeclient(client_fd,strcat(str,username));
			break;	
		
		case UNAUTH_USER:
			closeclient(client_fd,"unauthorised");
			break;
		default:
			closeclient(client_fd,"unauthorised");
			break;
	}
}
// Terminating the connection with the client 
void closeclient(int client_fd,char *str)
{
	sendMsgtoClient(client_fd, str);
    shutdown(client_fd, SHUT_RDWR);
}

int main(int argc,char **argv)
{
	int sock_fd,client_fd,port_no;
	struct sockaddr_in serv_addr, cli_addr;
	memset((void*)&serv_addr, 0, sizeof(serv_addr));
	port_no=55000;

	sock_fd=socket(AF_INET, SOCK_STREAM, 0);

	serv_addr.sin_port = htons(port_no);         //set the port number
	serv_addr.sin_family = AF_INET;             //setting DOMAIN
	serv_addr.sin_addr.s_addr = INADDR_ANY;     //permits any incoming IP

	if(bind(sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
	    printf("Error on binding.\n");
	    exit(EXIT_FAILURE);
	}
	int reuse=1;
	setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
	listen(sock_fd,5); 
	int clisize=sizeof(cli_addr);

	while(1) {
	    //blocking call
	    memset(&cli_addr, 0, sizeof(cli_addr));
	    if((client_fd = accept(sock_fd, (struct sockaddr*)&cli_addr, &clisize)) < 0) {
	        printf("Error on accept.\n");
	        exit(EXIT_FAILURE);
	    }

	    switch(fork()) {
	        case -1:
	            printf("Error in fork.\n");
	            break;
	        case 0: {
	            close(sock_fd);
	            talkToClient(client_fd);
	            exit(EXIT_SUCCESS);
	            break;
	        }
	        default:
	            close(client_fd);
	            break;
	    }
	}

}