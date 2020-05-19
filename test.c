#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include <fcntl.h>

int main(int argc, char *argv[]){

	char * test1Content = "This is a test file. Code goes in here.";
	char * test2Content = "This is another test file. This one is a bit longer. qwertyuio\nqwertyuio\nqwertyuio\nqwertyuio\nqwertyuio\nqwertyuio\nqwertyuio\nqwertyuio\nqwertyuio\nqwertyuio\nqwertyuio\n";
	char * test3Content = "I love CS214";
	char * test4Content = "This is a file in project2\n\n\n yay";
	char * test5Content = "I love github. This file is located in a subdirectory.";
	char * test6Content = "I get those goosebumps every time, yeah, you come around, yeah\nYou ease my mind, you make everything feel fine\nWorried 'bout those comments, I'm way too numb\nIt's way too dumb, yeah";
	char * test7Content = "This file was added later in development.";
	//build all files and directories
	system("mkdir project1");
	system("mkdir project2");
	system("project2/src");

	int test1 = open("project1/test1.txt", O_CREAT | O_RDWR | O_TRUNC ,0666);
	write(test1, test1Content, strlen(test1Content));

	int test2 = open("project1/test2.txt", O_CREAT | O_RDWR | O_TRUNC,0666);
	write(test2, test2Content, strlen(test2Content));

	int test3= open("project1/test3.txt", O_CREAT | O_RDWR | O_TRUNC,0666);
	write(test3, test3Content, strlen(test3Content));
	
	int test4 = open("project2/test4.txt", O_CREAT | O_RDWR | O_TRUNC ,0666);
	write(test4, test4Content, strlen(test4Content));
	
	int test5 = open("project2/src/test5.txt", O_CREAT | O_RDWR | O_TRUNC ,0666);
	write(test5, test5Content, strlen(test5Content));

	int test6 = open("project2/test6.txt", O_CREAT | O_RDWR | O_TRUNC ,0666);
	write(test6, test6Content, strlen(test6Content));
	

	system("mkdir repository");
	system("make");
	printf("Successfully built testing enviornment.\n");
	printf("IMPORTANT NOTE: Please run WTFServer before executing test cases by navigating into \"repository\" directory and typing \"./../WTFServer");
	//TEST CASE 1:
	printf("\nExecuting Test Case 1...\n");

	system("./WTF configure 127.0.0.1 3491");//.FUIX THIS!!!
	system("./WTF add project1 test1.txt");
	system("./WTF create project1");
	system("./WTF add nonexistantProject dummyFile.txt");
	system("./WTF remove testproj test.txt");
	printf("Expected output:\n.configure file and initialized manifest in project directory. Should give error messages for impossible commands.\n");

	printf("\nExecuting Test Case 2...\n");
	system("./WTF destroy project1");
	//TURN ON SERVER
	system("./WTF create project1");
	system("./WTF add project1 test1.txt");
	system("./WTF add project1 test2.txt");
	system("./WTF add project1 test3.txt");
	system("./WTF remove project1 test2.txt");
	system("./WTF remove project1 test2.txt");
	system("./WTF commit project1");
	system("./WTF commit project2");
	system("./WTF push project1");
	printf("Expected output:\n Sucess messages for all commands, except for failure on the 6th and 8th. Client's code should be successfully pushed to server.\n");

	printf("\nExecuting Test Case 3...\n");
	system("./WTF create project2");
	system("./WTF add project2 test4.txt");
	system("./WTF add project2 src/test5.txt");
	system("./WTF add project2 test6.txt");
	system("./WTF commit project2");
	system("./WTF push project2");
	printf("Expected output:\n Success messages for all commands and new project on server.\n");

	printf("\nExecuting Test Case 4...\n");
	//update various files
	char * update1 = "code code code";
	write(test2, update1, strlen(update1));
	char * update2 = "\ngrader->giveGoodGrade = 1;\nClass CS214 = malloc(sizeof(AmazingClass));";
	write(test4, update2, strlen(update2));
	int test7 = open("project2/test7.txt", O_CREAT | O_RDWR | O_TRUNC ,0666);
	write(test7, test7Content, strlen(test7Content));
	system("./WTF add project2 test7.txt");
	system("./WTF remove project1 test1.txt");
	system("./WTF commit project1");
	system("./WTF push project1");
	system("./WTF history project1");
	system("./WTF history project2");
	printf("Expected Output: \nUpdated project1 code on server, file7.txt added to project2 manifest, history of both projects.\n");

	printf("\nExecuting Test Case 5...\n");
	system("./WTF update project2");
	system("./WTF commit project2");
	system("./WTF push project2");
	system("./WTF update project2");
	system("./WTF currentversion project1");
	system("./WTF currentversion project2");
	printf("Expected Output: Expected Output:\nFirst update should fail, as user has modified files locally. Commit and push should execute successfully, then second update should inform the user that files are up to date. Currentversion commands should output current versions.\n");
	
	printf("\nExecuting Test Case 6...\n");

	close(test1);
	close(test2);
	close(test3);
	close(test4);
	close(test5);
	close(test6);
	close(test7);
}
