#include "frgg.h"

int
bad_request()
{
	puts("content-type: text/html; charset=gbk\n\n");
	puts("<html><head>");
	puts("<meta http-equiv='content-type' content='text/html;charset=gbk'>");
	puts("</head><body>");
	puts("<h3>bad request</h3>");
	puts("</body></html>");
	exit(1);
}

int get_filelist_board(char *bname)
{
    struct fileheader fh;    
    char dir[PATH_MAX];
    int fd;

    chdir(BBSHOME);    
    snprintf(dir, sizeof(dir), "boards/%s/.DIR", bname);
    
    if ((fd = open(dir, O_RDONLY)) < 0) {
        ERROR1("open %s fail", dir);
        return -1;
    }

    while(read(fd, &fh, sizeof(fh)) > 0) {
        printf("%s\n", fh.filename);
    }
    close(fd);

    return 0;
}

int get_filelist(char *bname)
{
    chdir(FRGGHOME);
    FILE *fp = fopen(BOARDS_LIST, "r");
    char board[PATH_MAX]; 
    
    if (fp == NULL) {
        ERROR("open BOARD_LIST fail");
        return -1;
    }
    while(fgets(board, sizeof(board), fp)) {
        if (board[strlen(board)-1] == '\n')
            board[strlen(board)-1] = '\0';
        if (strcmp(board, bname) == 0) {
            get_filelist_board(bname); 
            goto final;
        }
    }
    
final:
    fclose(fp);
    return 0;
}
int main(int argc, char *argv[])
{
    char *raw_query_string = getenv("QUERY_STRING");

    if (raw_query_string == NULL) {
        bad_request();
    }
    char *query = raw_query_string;
    if (strncmp(query, "board=", 6) != 0) {
        bad_request();
    }

    query += 6;
    char *endq = query;
    while(*endq && isalpha(*endq)) endq++;
    *endq = '\0';

    get_filelist(query);

    return 0;
}
