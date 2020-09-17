/* ************************************************************************
*   File: newzerkalo.h                            Part of NewZerkalo MUD  *
*  Usage: prool subprograms for NewZerkalo MUD                            *
*                                                                         *
*  (CC) 2012-2020 Prool                                                   *
*                                                                         *
*  Author: Prool, proolix@gmail.com, http://prool.kharkov.org             *
************************************************************************ */

#define PROOL_G_LEN 1024
#define PROOL_MAX_STRLEN 1024
#define BUFLEN 512

#define DUKHMADA_FILE "dukhmada.cfg"
#define NABORY_FILE "nabory.cfg"
#define PERSLOG_FILE "../log/perslog.txt"

extern char prool_g_buf[];

char *ptime(void);
char	*to_utf(char *str);
void make_who2html(void);
int can_take_obj(CHAR_DATA * ch, OBJ_DATA * obj);
void perslog (char *verb, const char *pers);

// prool commands:
void do_fflush(CHAR_DATA *ch, char *argument, int cmd, int subcmd);
void do_dukhmada(CHAR_DATA *ch, char *argument, int cmd, int subcmd);
void do_get_nabor(CHAR_DATA *ch, char *argument, int cmd, int subcmd);
void do_accio_trup(CHAR_DATA *ch, char *argument, int cmd, int subcmd);
void do_shutdown_info (CHAR_DATA *ch, char * /*argument*/, int/* cmd*/, int/* subcmd*/);
void do_kogda (CHAR_DATA *ch, char *argument, int/* cmd*/, int/* subcmd*/);
void do_proolflag (CHAR_DATA *ch, char *arg, int/* cmd*/, int/* subcmd*/);
// end of prool commands
