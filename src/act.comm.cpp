/*************************************************************************
*   File: act.comm.cpp                                  Part of Bylins    *
*  Usage: Player-level communication commands                             *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
*                                                                         *
*  $Author$                                                        *
*  $Date$                                           *
*  $Revision$                                                       *
************************************************************************ */

#include "conf.h"
#include <sstream>
#include <list>
#include <string>
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "screen.h"
#include "dg_scripts.h"
#include "auction.h"
#include "privilege.hpp"
#include "char.hpp"

/* extern variables */
extern DESCRIPTOR_DATA *descriptor_list;
extern CHAR_DATA *character_list;
extern TIME_INFO_DATA time_info;

/* local functions */
void perform_tell(CHAR_DATA * ch, CHAR_DATA * vict, char *arg);
int is_tell_ok(CHAR_DATA * ch, CHAR_DATA * vict);
static char remember_pray[MAX_REMEMBER_PRAY][MAX_INPUT_LENGTH];
static int num_pray = 0;
//MZ.gossip_fix
#define REMEMBER_TIME_LENGTH 24
#define PREFIX_LENGTH 4
#if PREFIX_LENGTH == 4
#define HOLLER_PREFIX (" &Y'")
#define GOSSIP_PREFIX (" &y'")
#endif
#define SUFFIX_LENGTH 3
#if SUFFIX_LENGTH == 3
#define SUFFIX ("'&n")
#endif
static char remember_gossip[MAX_REMEMBER_GOSSIP]
	    [(REMEMBER_TIME_LENGTH - 1) + PREFIX_LENGTH + MAX_INPUT_LENGTH + SUFFIX_LENGTH];
//-MZ.gossip_fix
static int num_gossip = 0;

/* external functions */
extern char *diag_timer_to_char(OBJ_DATA * obj);
extern void set_wait(CHAR_DATA * ch, int waittime, int victim_in_room);

ACMD(do_say);
ACMD(do_gsay);
ACMD(do_tell);
ACMD(do_reply);
ACMD(do_spec_comm);
ACMD(do_write);
ACMD(do_page);
ACMD(do_gen_comm);
ACMD(do_pray_gods);
ACMD(do_remember_char);
// shapirus
ACMD(do_ignore);

#define SIELENCE ("�� ����, ��� ���� �� ���.\r\n")


ACMD(do_say)
{
	skip_spaces(&argument);
	CHAR_DATA *to;

	if (AFF_FLAGGED(ch, AFF_SIELENCE)) {
		send_to_char(SIELENCE, ch);
		return;
	}

	if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_DUMB)) {
		send_to_char("��� ��������� ���������� � ������ �������!\r\n", ch);
		return;
	}

	if (!*argument)
		send_to_char("�� ����������: \"���� �� ������ �������?\"\r\n", ch);
	else {
		sprintf(buf, "$n ������$g : '%s'", argument);
//      act (buf, FALSE, ch, 0, 0, TO_ROOM | DG_NO_TRIG | CHECK_DEAF);
// shapirus; ��� ����������� ������������� ������ � ������
// �������� �������� act � ������ �� ������ �� ������
		for (to = world[ch->in_room]->people; to; to = to->next_in_room) {
			if (ch == to || ignores(to, ch, IGNORE_SAY))
				continue;
			act(buf, FALSE, ch, 0, to, TO_VICT | DG_NO_TRIG | CHECK_DEAF);
		}
		if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_NOREPEAT))
			send_to_char(OK, ch);
		else {
			delete_doubledollar(argument);
			sprintf(buf, "�� ������� : '%s'\r\n", argument);
			send_to_char(buf, ch);
		}
		speech_mtrigger(ch, argument);
		speech_wtrigger(ch, argument);
	}
}


ACMD(do_gsay)
{
	CHAR_DATA *k;
	struct follow_type *f;

	if (AFF_FLAGGED(ch, AFF_SIELENCE)) {
		send_to_char(SIELENCE, ch);
		return;
	}

	if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_DUMB)) {
		send_to_char("��� ��������� ���������� � ������ �������!\r\n", ch);
		return;
	}

	skip_spaces(&argument);

	if (!AFF_FLAGGED(ch, AFF_GROUP)) {
		send_to_char("�� �� ��������� ������ ������ !\r\n", ch);
		return;
	}
	if (!*argument)
		send_to_char("� ��� �� ������ �������� ����� ������ ?\r\n", ch);
	else {
		if (ch->master)
			k = ch->master;
		else
			k = ch;

		sprintf(buf, "$n �������$g ������ : '%s'", argument);

		if (AFF_FLAGGED(k, AFF_GROUP) && (k != ch) && !ignores(k, ch, IGNORE_GROUP))
			act(buf, FALSE, ch, 0, k, TO_VICT | TO_SLEEP | CHECK_DEAF);
		for (f = k->followers; f; f = f->next)
			if (AFF_FLAGGED(f->follower, AFF_GROUP) && (f->follower != ch) &&
			    !ignores(f->follower, ch, IGNORE_GROUP))
				act(buf, FALSE, ch, 0, f->follower, TO_VICT | TO_SLEEP | CHECK_DEAF);

		if (PRF_FLAGGED(ch, PRF_NOREPEAT))
			send_to_char(OK, ch);
		else {
			sprintf(buf, "�� �������� ������ : '%s'\r\n", argument);
			send_to_char(buf, ch);
		}
	}
}


void perform_tell(CHAR_DATA * ch, CHAR_DATA * vict, char *arg)
{
	time_t ct;
	char *tmp;

// shapirus: �� �������� ������, ���� ������ �� ����� � ��������
// ��������������� �����; ��������� ����� ������ ������
	if (PRF_FLAGGED(vict, PRF_NOINVISTELL)
		&& !CAN_SEE(vict, ch)
		&& GET_LEVEL(ch) < LVL_IMMORT
		&& !PRF_FLAGGED(ch, PRF_CODERINFO))
	{
		act("$N �� ����� ������������� � ����, ���� �� �����.", FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
		return;
	}

	// TODO: ���� � act() ��������� ����� �����, �� ��� � ��� ���� ���������� �� act()
	if (CAN_SEE_CHAR(vict, ch) || IS_IMMORTAL(ch) || GET_INVIS_LEV(ch))
		sprintf(buf, "%s ������%s ��� : '%s'", GET_NAME(ch), GET_CH_SUF_1(ch), arg);
	else
		sprintf(buf, "���-�� ������ ��� : '%s'", arg);
	send_to_char(vict, "%s%s%s\r\n", CCICYN(vict, C_NRM), CAP(buf), CCNRM(vict, C_NRM));

	/* ��������� ��� "���������" */
	arg[MAX_RAW_INPUT_LENGTH - 35] = 0;
	if (!IS_NPC(vict) && !IS_NPC(ch)) {
		ct = time(0);
		tmp = asctime(localtime(&ct));
		if (CAN_SEE_CHAR(vict, ch) || IS_IMMORTAL(ch) || GET_INVIS_LEV(ch)) {
			sprintf(buf, "%s[%5.5s]%s %s : '%s'%s", CCNRM(ch, C_NRM), (tmp + 11), CCICYN(ch, C_NRM),
				GET_NAME(ch), arg, CCNRM(ch, C_NRM));
		} else {
			sprintf(buf, "%s[%5.5s]%s ���-�� : '%s'%s", CCNRM(ch, C_NRM), (tmp + 11), CCICYN(ch, C_NRM),
				arg, CCNRM(ch, C_NRM));
		}
		strcpy(GET_TELL(vict, GET_LASTTELL(vict)), buf);
		GET_LASTTELL(vict)++;
		if (GET_LASTTELL(vict) == MAX_REMEMBER_TELLS)
			GET_LASTTELL(vict) = 0;
	}

	if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_NOREPEAT))
		send_to_char(OK, ch);
	else {
		if (CAN_SEE_CHAR(ch, vict) || IS_IMMORTAL(vict) || GET_INVIS_LEV(vict))
			sprintf(buf, "�� ������� %s : '%s'", vict->player.PNames[2], arg);
		else
			sprintf(buf, "�� ������� ����-�� : '%s'", arg);
		send_to_char(ch, "%s%s%s\r\n", CCICYN(ch, C_NRM), buf, CCNRM(ch, C_NRM));
	}

	if (!IS_NPC(vict) && !IS_NPC(ch))
		GET_LAST_TELL(vict) = GET_IDNUM(ch);
}

int is_tell_ok(CHAR_DATA * ch, CHAR_DATA * vict)
{
	if (ch == vict) {
		send_to_char("�� ������ ���������� ������������� � ����� �����.\r\n", ch);
		return (FALSE);
	} else if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_DUMB)) {
		send_to_char("��� ��������� ���������� � ������ �������.\r\n", ch);
		return (FALSE);
	} else if (!IS_NPC(vict) && !vict->desc) {	/* linkless */
		act("$N �������$G ����� � ���� ������.", FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
		return (FALSE);
	} else if (PLR_FLAGGED(vict, PLR_WRITING)) {
		act("$N ����� ��������� - ��������� �������.", FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
		return (FALSE);
	}

	if (IS_GOD(ch) || Privilege::check_flag(ch, Privilege::KRODER))
		return (TRUE);

	if (ROOM_FLAGGED(ch->in_room, ROOM_SOUNDPROOF))
		send_to_char("����� ��������� ���� �����.\r\n", ch);
	else if ((!IS_NPC(vict) &&
		  (PRF_FLAGGED(vict, PRF_NOTELL) || ignores(vict, ch, IGNORE_TELL))) ||
		 ROOM_FLAGGED(vict->in_room, ROOM_SOUNDPROOF))
		act("$N �� ������ ��� ��������.", FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
	else if (GET_POS(vict) < POS_RESTING || AFF_FLAGGED(vict, AFF_DEAFNESS))
		act("$N ��� �� �������.", FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
	else
		return (TRUE);

	return (FALSE);
}

/*
 * Yes, do_tell probably could be combined with whisper and ask, but
 * called frequently, and should IMHO be kept as tight as possible.
 */
ACMD(do_tell)
{
	CHAR_DATA *vict = NULL;

	if (AFF_FLAGGED(ch, AFF_CHARM))
		return;

	if (AFF_FLAGGED(ch, AFF_SIELENCE)) {
		send_to_char(SIELENCE, ch);
		return;
	}

	half_chop(argument, buf, buf2);

	if (!*buf || !*buf2) {
		send_to_char("��� � ���� �� ������ �������?\r\n", ch);
	} else if (!(vict = get_player_vis(ch, buf, FIND_CHAR_WORLD))) {
		send_to_char(NOPERSON, ch);
	} else if (IS_NPC(vict))
		send_to_char(NOPERSON, ch);
	else if (is_tell_ok(ch, vict)) {
		if (PRF_FLAGGED(ch, PRF_NOTELL))
			send_to_char("�������� ��� �� ������!\r\n", ch);
		perform_tell(ch, vict, buf2);
	}
}


ACMD(do_reply)
{
	CHAR_DATA *tch = character_list;

	if (IS_NPC(ch))
		return;

	if (AFF_FLAGGED(ch, AFF_SIELENCE)) {
		send_to_char(SIELENCE, ch);
		return;
	}

	if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_DUMB)) {
		send_to_char("��� ��������� ���������� � ������ �������!\r\n", ch);
		return;
	}

	skip_spaces(&argument);

	if (GET_LAST_TELL(ch) == NOBODY)
		send_to_char("��� ������ �������� !\r\n", ch);
	else if (!*argument)
		send_to_char("��� �� ����������� ��������?\r\n", ch);
	else {			/*
				 * Make sure the person you're replying to is still playing by searching
				 * for them.  Note, now last tell is stored as player IDnum instead of
				 * a pointer, which is much better because it's safer, plus will still
				 * work if someone logs out and back in again.
				 */

		/*
		 * XXX: A descriptor list based search would be faster although
		 *      we could not find link dead people.  Not that they can
		 *      hear tells anyway. :) -gg 2/24/98
		 */
		while (tch != NULL && (IS_NPC(tch) || GET_IDNUM(tch) != GET_LAST_TELL(ch)))
			tch = tch->next;

		if (tch == NULL)
			send_to_char("����� ������ ��� ��� � ����.", ch);
		else if (is_tell_ok(ch, tch))
			perform_tell(ch, tch, argument);
	}
}


ACMD(do_spec_comm)
{
	CHAR_DATA *vict;
	const char *action_sing, *action_plur, *action_others, *vict1, *vict2;
	char vict3[MAX_INPUT_LENGTH];

	if (AFF_FLAGGED(ch, AFF_SIELENCE)) {
		send_to_char(SIELENCE, ch);
		return;
	}

	if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_DUMB)) {
		send_to_char("��� ��������� ���������� � ������ �������!\r\n", ch);
		return;
	}

	if (subcmd == SCMD_WHISPER) {
		action_sing = "�������";
		vict1 = "����";
		vict2 = "���";
		action_plur = "���������";
		action_others = "$n ���-�� ���������$g $N2.";
	} else {
		action_sing = "��������";
		vict1 = "� ����";
		vict2 = "� ���";
		action_plur = "�������";
		action_others = "$n �����$g $N2 ������.";
	}

	half_chop(argument, buf, buf2);

	if (!*buf || !*buf2) {
		sprintf(buf, "��� �� ������ %s.. � %s?\r\n", action_sing, vict1);
		send_to_char(buf, ch);
	} else if (!(vict = get_char_vis(ch, buf, FIND_CHAR_ROOM)))
		send_to_char(NOPERSON, ch);
	else if (vict == ch)
		send_to_char("�� ����� ��� �� ���� - ����� ���� ������...\r\n", ch);
	else if (ignores(vict, ch, subcmd == SCMD_WHISPER ? IGNORE_WHISPER : IGNORE_ASK)) {
		sprintf(buf, "%s �� ������ ��� �������.\r\n", GET_NAME(vict));
		send_to_char(buf, ch);
	} else {
		if (subcmd == SCMD_WHISPER)
			sprintf(vict3, "%s", GET_PAD(vict, 2));
		else
			sprintf(vict3, "� %s", GET_PAD(vict, 1));

		sprintf(buf, "$n %s$g %s : '%s'", action_plur, vict2, buf2);
		act(buf, FALSE, ch, 0, vict, TO_VICT | CHECK_DEAF);

		if (PRF_FLAGGED(ch, PRF_NOREPEAT))
			send_to_char(OK, ch);
		else {
			sprintf(buf, "�� %s� %s : '%s'\r\n", action_plur, vict3, buf2);
			send_to_char(buf, ch);
		}

		act(action_others, FALSE, ch, 0, vict, TO_NOTVICT);
	}
}



#define MAX_NOTE_LENGTH 1000	/* arbitrary */

ACMD(do_write)
{
	OBJ_DATA *paper, *pen = NULL;
	char *papername, *penname;

	papername = buf1;
	penname = buf2;

	two_arguments(argument, papername, penname);

	if (!ch->desc)
		return;

	if (!*papername) {	/* nothing was delivered */
		send_to_char("��������?  ���?  � �� ���?\r\n", ch);
		return;
	}
	if (*penname) {		/* there were two arguments */
		if (!(paper = get_obj_in_list_vis(ch, papername, ch->carrying))) {
			sprintf(buf, "� ��� ��� %s.\r\n", papername);
			send_to_char(buf, ch);
			return;
		}
		if (!(pen = get_obj_in_list_vis(ch, penname, ch->carrying))) {
			sprintf(buf, "� ��� ��� %s.\r\n", penname);
			send_to_char(buf, ch);
			return;
		}
	} else {		/* there was one arg.. let's see what we can find */
		if (!(paper = get_obj_in_list_vis(ch, papername, ch->carrying))) {
			sprintf(buf, "�� �� ������ %s � ���������.\r\n", papername);
			send_to_char(buf, ch);
			return;
		}
		if (GET_OBJ_TYPE(paper) == ITEM_PEN) {	/* oops, a pen.. */
			pen = paper;
			paper = NULL;
		} else if (GET_OBJ_TYPE(paper) != ITEM_NOTE) {
			send_to_char("�� �� ������ �� ���� ������.\r\n", ch);
			return;
		}
		/* One object was found.. now for the other one. */
		if (!GET_EQ(ch, WEAR_HOLD)) {
			sprintf(buf, "�� ����� ������!\r\n");
			send_to_char(buf, ch);
			return;
		}
		if (!CAN_SEE_OBJ(ch, GET_EQ(ch, WEAR_HOLD))) {
			send_to_char("�� ������� ���-�� ���������!  ����, �� ������ ���� ������!!\r\n", ch);
			return;
		}
		if (pen)
			paper = GET_EQ(ch, WEAR_HOLD);
		else
			pen = GET_EQ(ch, WEAR_HOLD);
	}


	/* ok.. now let's see what kind of stuff we've found */
	if (GET_OBJ_TYPE(pen) != ITEM_PEN)
		act("�� �� ������ ������ $o4.", FALSE, ch, pen, 0, TO_CHAR);
	else if (GET_OBJ_TYPE(paper) != ITEM_NOTE)
		act("�� �� ������ ������ �� $o5.", FALSE, ch, paper, 0, TO_CHAR);
	else if (paper->action_description)
		send_to_char("��� ��� ���-�� ��������.\r\n", ch);
	else {			/* we can write - hooray! */
		/* this is the PERFECT code example of how to set up:
		 * a) the text editor with a message already loaed
		 * b) the abort buffer if the player aborts the message
		 */
		ch->desc->backstr = NULL;
		send_to_char("������ ������.  (/s ��������� ������  /h ������)\r\n", ch);
		/* ok, here we check for a message ALREADY on the paper */
		if (paper->action_description) {	/* we str_dup the original text to the descriptors->backstr */
			ch->desc->backstr = str_dup(paper->action_description);
			/* send to the player what was on the paper (cause this is already */
			/* loaded into the editor) */
			send_to_char(paper->action_description, ch);
		}
		act("$n �����$g ������.", TRUE, ch, 0, 0, TO_ROOM);
		/* assign the descriptor's->str the value of the pointer to the text */
		/* pointer so that we can reallocate as needed (hopefully that made */
		/* sense :>) */
		string_write(ch->desc, &paper->action_description, MAX_NOTE_LENGTH, 0, NULL);
	}
}

ACMD(do_page)
{
	DESCRIPTOR_DATA *d;
	CHAR_DATA *vict;

	half_chop(argument, arg, buf2);

	if (IS_NPC(ch))
		send_to_char("��������-��-��������� ����� �� �����.. ��������.\r\n", ch);
	else if (!*arg)
		send_to_char("Whom do you wish to page?\r\n", ch);
	else {
		sprintf(buf, "\007\007*$n* %s", buf2);
		if (!str_cmp(arg, "all") || !str_cmp(arg, "���")) {
			if (IS_GRGOD(ch)) {
				for (d = descriptor_list; d; d = d->next)
					if (STATE(d) == CON_PLAYING && d->character)
						act(buf, FALSE, ch, 0, d->character, TO_VICT);
			} else
				send_to_char("��� �������� ������ ����� !\r\n", ch);
			return;
		}
		if ((vict = get_char_vis(ch, arg, FIND_CHAR_WORLD)) != NULL) {
			act(buf, FALSE, ch, 0, vict, TO_VICT);
			if (PRF_FLAGGED(ch, PRF_NOREPEAT))
				send_to_char(OK, ch);
			else
				act(buf, FALSE, ch, 0, vict, TO_CHAR);
		} else
			send_to_char("����� ����� ����������� !\r\n", ch);
	}
}


/**********************************************************************
 * generalized communication func, originally by Fred C. Merkel (Torg) *
  *********************************************************************/

struct communication_type {
	const char *muted_msg;
	const char *action;
	const char *no_channel;
	const char *color;
	const char *you_action;
	const char *hi_action;
	int min_lev;
	int move_cost;
	int noflag;
};

ACMD(do_gen_comm)
{
	DESCRIPTOR_DATA *i;
	char color_on[24];
//MZ.gossip_fix
	char remember_time[REMEMBER_TIME_LENGTH];
//-MZ.gossip_fix
	int ign_flag;
	/*
	 * com_msgs: Message if you can't perform the action because of mute
	 *           name of the action
	 *           message if you're not on the channel
	 *           a color string.
	 *           �� ....
	 *           ��(�) ....
	 *           min access level.
	 *           mov cost.
	 */

	struct communication_type com_msgs[] = {
		{"�� �� ������ �����.\r\n",	/* holler */
		 "�����",
		 "�� ��� ��������� ������.",
		 KIYEL,
		 "�������",
		 "������$g",
		 4,
		 25,
		 PRF_NOHOLLER},

		{"��� ��������� �������.\r\n",	/* shout */
		 "�������",
		 "�� ��� ��������� ������.\r\n",
		 KIYEL,
		 "���������",
		 "��������$g",
		 2,
		 10,
		 PRF_NOSHOUT},

		{"��� ����������� �������.\r\n",	/* gossip */
		 "�������",
		 "�� ��� ��������� ������.\r\n",
		 KYEL,
		 "��������",
		 "�������$g",
		 3,
		 15,
		 PRF_NOGOSS},

		{"��� �� � ���� �����������.\r\n",	/* auction */
		 "���������",
		 "�� ��� ��������� ������.\r\n",
		 KIYEL,
		 "����������� �������������",
		 "�������$g � ����",
		 2,
		 0,
		 PRF_NOAUCT},
	};

	/* to keep pets, etc from being ordered to shout */
//  if (!ch->desc)
	if (AFF_FLAGGED(ch, AFF_CHARM))
		return;

	if (AFF_FLAGGED(ch, AFF_SIELENCE)) {
		send_to_char(SIELENCE, ch);
		return;
	}

	if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_DUMB)) {
		send_to_char("��� ��������� ���������� � ������ �������!\r\n", ch);
		return;
	}

	if (PLR_FLAGGED(ch, PLR_MUTE) && subcmd != SCMD_AUCTION) {
		send_to_char(com_msgs[subcmd].muted_msg, ch);
		return;
	}
	if (ROOM_FLAGGED(ch->in_room, ROOM_SOUNDPROOF)) {
		send_to_char("����� ��������� ���� �����.\r\n", ch);
		return;
	}

	if (GET_LEVEL(ch) < com_msgs[subcmd].min_lev && !GET_REMORT(ch)) {
		sprintf(buf1,
			"��� ����� ������� ���� �� %d ������, ����� �� ����� %s.\r\n",
			com_msgs[subcmd].min_lev, com_msgs[subcmd].action);
		send_to_char(buf1, ch);
		return;
	}

	/* make sure the char is on the channel */
	if (PRF_FLAGGED(ch, com_msgs[subcmd].noflag)) {
		send_to_char(com_msgs[subcmd].no_channel, ch);
		return;
	}

	/* skip leading spaces */
	skip_spaces(&argument);

	/* make sure that there is something there to say! */
	if (!*argument && subcmd != SCMD_AUCTION) {
		sprintf(buf1, "����� ! ��, ����� ��� ������, ��� %s ???\r\n", com_msgs[subcmd].action);
		send_to_char(buf1, ch);
		return;
	}

	/* set up the color on code */
	strcpy(color_on, com_msgs[subcmd].color);

	/* ����-������� :coded by ����� */

#define MAX_UPPERS_CHAR_PRC 30
#define MAX_UPPERS_SEQ_CHAR 3

	if ((subcmd != SCMD_AUCTION) && (!IS_IMMORTAL(ch)) && (!IS_NPC(ch))) {
		unsigned int bad_smb_procent = MAX_UPPERS_CHAR_PRC;
		int bad_simb_cnt = 0, bad_seq_cnt = 0;

		/* ��������� ������� ������� */
		for (int k = 0; argument[k] != '\0'; k++) {
			if (a_isupper(argument[k])) {
				bad_simb_cnt++;
				bad_seq_cnt++;
			} else
				bad_seq_cnt = 0;

			if ((bad_seq_cnt > 1) &&
			    (((bad_simb_cnt * 100 / strlen(argument)) > bad_smb_procent) ||
			     (bad_seq_cnt > MAX_UPPERS_SEQ_CHAR)))
				argument[k] = a_lcc(argument[k]);
		}
		/* ��������� ���������� ��������� � ����� */
		if (GET_LAST_ALL_TELL(ch) == NULL) {
			GET_LAST_ALL_TELL(ch) = (char *) malloc(strlen(argument) + 1);
		} else {
			GET_LAST_ALL_TELL(ch) = (char *) realloc(GET_LAST_ALL_TELL(ch), strlen(argument) + 1);
		}

		if (!strcmp(GET_LAST_ALL_TELL(ch), argument)) {
			send_to_char("�� �� �� ������� �������� ���� �����!?!\r\n", ch);
			return;
		}
		strcpy(GET_LAST_ALL_TELL(ch), argument);
	}

	// � ���� �������� ������ ����������� ���� �� �����, ������� ��� ������ ���� ���������
	if (!check_moves(ch, com_msgs[subcmd].move_cost))
		return;

	/* first, set up strings to be given to the communicator */
	if (subcmd == SCMD_AUCTION) {
		*buf = '\0';
		auction_drive(ch, argument);
		return;
	} else {
		if (PRF_FLAGGED(ch, PRF_NOREPEAT))
			send_to_char(OK, ch);
		else {
			if (COLOR_LEV(ch) >= C_CMP)
				sprintf(buf1, "%s�� %s : '%s'%s", color_on,
					com_msgs[subcmd].you_action, argument, KNRM);
			else
				sprintf(buf1, "�� %s : '%s'", com_msgs[subcmd].you_action, argument);
			act(buf1, FALSE, ch, 0, 0, TO_CHAR | TO_SLEEP);
		}

		sprintf(buf, "$n %s : '%s'", com_msgs[subcmd].hi_action, argument);
		// ��������� ��� "��������� �����"
		switch (time_info.hours % 24) {
		case 0:
			sprintf(remember_time, "[�������]");
			break;
		case 1:
			sprintf(remember_time, "[1 ��� ����]");
			break;
		case 2:
		case 3:
		case 4:
			sprintf(remember_time, "[%d ���� ����]", time_info.hours);
			break;
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
			sprintf(remember_time, "[%d ����� ����]", time_info.hours);
			break;
		case 12:
			sprintf(remember_time, "[�������]");
			break;
		case 13:
			sprintf(remember_time, "[1 ��� ���������]");
			break;
		case 14:
		case 15:
		case 16:
			sprintf(remember_time, "[%d ���� ���������]", time_info.hours - 12);
			break;
		case 17:
		case 18:
		case 19:
		case 20:
		case 21:
		case 22:
		case 23:
			sprintf(remember_time, "[%d ����� ������]", time_info.hours - 12);
			break;
		}
		if (subcmd == SCMD_HOLLER) {
//MZ.gossip_fix
			sprintf(remember_gossip[num_gossip], "%s%s%s%s", remember_time, HOLLER_PREFIX, argument, SUFFIX);
//-MZ.gossip_fix
			num_gossip++;
			if (num_gossip == MAX_REMEMBER_GOSSIP)
				num_gossip = 0;
		}
		if (subcmd == SCMD_GOSSIP) {
//MZ.gossip_fix
			sprintf(remember_gossip[num_gossip], "%s%s%s%s", remember_time, GOSSIP_PREFIX, argument, SUFFIX);
//-MZ.gossip_fix
			num_gossip++;
			if (num_gossip == MAX_REMEMBER_GOSSIP)
				num_gossip = 0;
		}
	}

	switch (subcmd) {
	case SCMD_SHOUT:
		ign_flag = IGNORE_SHOUT;
		break;
	case SCMD_GOSSIP:
		ign_flag = IGNORE_GOSSIP;
		break;
	case SCMD_HOLLER:
		ign_flag = IGNORE_HOLLER;
		break;
	default:
		ign_flag = 0;
	}

	/* now send all the strings out */
	for (i = descriptor_list; i; i = i->next) {
		if (STATE(i) == CON_PLAYING && i != ch->desc && i->character &&
		    !PRF_FLAGGED(i->character, com_msgs[subcmd].noflag) &&
		    !PLR_FLAGGED(i->character, PLR_WRITING) &&
		    !ROOM_FLAGGED(i->character->in_room, ROOM_SOUNDPROOF) && GET_POS(i->character) > POS_SLEEPING) {
			if (ignores(i->character, ch, ign_flag))
				continue;
			if (subcmd == SCMD_SHOUT &&
			    ((world[ch->in_room]->zone != world[i->character->in_room]->zone) || !AWAKE(i->character)))
				continue;
			if (COLOR_LEV(i->character) >= C_NRM)
				send_to_char(color_on, i->character);
			act(buf, FALSE, ch, 0, i->character, TO_VICT | TO_SLEEP | CHECK_DEAF);
			if (COLOR_LEV(i->character) >= C_NRM)
				send_to_char(KNRM, i->character);
		}
	}
}


ACMD(do_mobshout)
{
	DESCRIPTOR_DATA *i;

	/* to keep pets, etc from being ordered to shout */
	if (!(IS_NPC(ch) || WAITLESS(ch)))
		return;
	if (AFF_FLAGGED(ch, AFF_CHARM))
		return;
	sprintf(buf, "$n ������$g : '%s'", argument);

	/* now send all the strings out */
	for (i = descriptor_list; i; i = i->next) {
		if (STATE(i) == CON_PLAYING && i->character &&
		    !PLR_FLAGGED(i->character, PLR_WRITING) && GET_POS(i->character) > POS_SLEEPING) {
			if (COLOR_LEV(i->character) >= C_NRM)
				send_to_char(KIYEL, i->character);
			act(buf, FALSE, ch, 0, i->character, TO_VICT | TO_SLEEP | CHECK_DEAF);
			if (COLOR_LEV(i->character) >= C_NRM)
				send_to_char(KNRM, i->character);
		}
	}
}

ACMD(do_pray_gods)
{
	char arg1[MAX_INPUT_LENGTH];
	char *tmp;
	DESCRIPTOR_DATA *i;
	CHAR_DATA *victim = NULL;
	time_t ct;

	skip_spaces(&argument);

	if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_DUMB)) {
		send_to_char("��� ��������� ���������� � �����, �������� �� �� ��������...\r\n", ch);
		return;
	}

	if (IS_IMMORTAL(ch)) {
		/* �������� ���� ���� �������� ���� */
		argument = one_argument(argument, arg1);
		skip_spaces(&argument);
		if (!*arg1) {
			send_to_char("������ ��������� �� ����������� ��������?\r\n", ch);
			return;
		}
		victim = get_player_vis(ch, arg1, FIND_CHAR_WORLD);
		if (victim == NULL) {
			send_to_char("������ ��� � ����!\r\n", ch);
			return;
		}
	}

	if (!*argument) {
		sprintf(buf, "� ��� �� ������ ���������� � �����?\r\n");
		send_to_char(buf, ch);
		return;
	}
	if (PRF_FLAGGED(ch, PRF_NOREPEAT))
		send_to_char(OK, ch);
	else {
		if (IS_NPC(ch))
			return;
		if (IS_IMMORTAL(ch)) {
			sprintf(buf, "&R�� ������� ������ %s : '%s'&n", GET_PAD(victim, 3), argument);
		} else {
			sprintf(buf, "&R�� �������� � ����� � ���������� : '%s'&n", argument);
			set_wait(ch, 3, FALSE);
		}
		act(buf, FALSE, ch, 0, argument, TO_CHAR);
	}

	if (IS_IMMORTAL(ch)) {
		sprintf(buf, "&R%s �������%s ��� : '%s'&n\r\n", GET_NAME(ch), GET_CH_SUF_1(ch), argument);
		send_to_char(buf, victim);
		sprintf(buf, "&R%s �������%s �� ��������� %s : '%s'&n", GET_NAME(ch),
			GET_CH_SUF_1(ch), GET_PAD(victim, 1), argument);
// ��������� ��� "��������� ��������"
		ct = time(0);
		tmp = asctime(localtime(&ct));
//MZ.pray_fix
		snprintf(remember_pray[num_pray], MAX_INPUT_LENGTH - SUFFIX_LENGTH,
			"&w[%5.5s] &R%s �������%s %s : '%s", (tmp + 11),
			GET_NAME(ch), GET_CH_SUF_1(ch), GET_PAD(victim, 2), argument);
		sprintf(remember_pray[num_pray] + strlen(remember_pray[num_pray]), "%s", SUFFIX);
//-MZ.pray_fix
		num_pray++;
		if (num_pray == MAX_REMEMBER_PRAY)
			num_pray = 0;
	} else {
		sprintf(buf, "&R$n �������$g � ����� � ���������� : '%s'&n", argument);
// ��������� ��� "��������� ��������"
		ct = time(0);
		tmp = asctime(localtime(&ct));
//MZ.pray_fix
		snprintf(remember_pray[num_pray], MAX_INPUT_LENGTH - SUFFIX_LENGTH,
			"&w[%5.5s] &R%s �������%s � ����� : '%s", (tmp + 11),
			GET_NAME(ch), GET_CH_SUF_1(ch), argument);
		sprintf(remember_pray[num_pray] + strlen(remember_pray[num_pray]), "%s", SUFFIX);
//-MZ.pray_fix
		num_pray++;
		if (num_pray == MAX_REMEMBER_PRAY)
			num_pray = 0;
	}
	for (i = descriptor_list; i; i = i->next)
	{
		if (STATE(i) == CON_PLAYING
			&& IS_IMMORTAL(i->character)
//			&& Privilege::god_list_check(GET_NAME(i->character), GET_UNIQUE(i->character))
			&& i->character != ch)
		{
			act(buf, 0, ch, 0, i->character, TO_VICT | TO_SLEEP);
		}
	}
}

static const int min_offtop_level = 6;
static const unsigned int max_remember_offtop = 15;
static std::list<std::string> remember_offtop;

/**
* ����� ������. �� ����� �����, ������ ����� ��� �������, ���/���� ����� ������.
*/
ACMD(do_offtop)
{
	if (IS_NPC(ch) || GET_LEVEL(ch) >= LVL_IMMORT)
	{
		send_to_char("���� ?\r\n", ch);
		return;
	}

	if (PLR_FLAGGED(ch, PLR_DUMB))
	{
		send_to_char("��� ��������� ���������� � ������ �������!\r\n", ch);
		return;
	}
	if (ROOM_FLAGGED(ch->in_room, ROOM_SOUNDPROOF))
	{
		send_to_char("����� ��������� ���� �����.\r\n", ch);
		return;
	}
	if (GET_LEVEL(ch) < min_offtop_level && !GET_REMORT(ch))
	{
		send_to_char(ch, "��� ����� ������� ���� �� %d ������, ����� �� ����� ���������.\r\n", min_offtop_level);
		return;
	}
	if (!PRF_FLAGGED(ch, PRF_OFFTOP_MODE))
	{
		send_to_char("�� ��� ��������� ������.\r\n", ch);
		return;
	}

	skip_spaces(&argument);
	if (!*argument)
	{
		send_to_char(ch, "��� ������ �������, �� ����� �� ���������.");
		return;
	}
	lower_convert(argument);

	if (GET_LAST_ALL_TELL(ch) && !strcmp(GET_LAST_ALL_TELL(ch), argument))
	{
		send_to_char("�� �� �� ������� �������� ���� �����!?!\r\n", ch);
		return;
	}

	if (!GET_LAST_ALL_TELL(ch))
		CREATE(GET_LAST_ALL_TELL(ch), char, strlen(argument) + 1);
	else
		RECREATE(GET_LAST_ALL_TELL(ch), char, strlen(argument) + 1);

	strcpy(GET_LAST_ALL_TELL(ch), argument);

	std::stringstream text;
	text << "[������] " << GET_NAME(ch) << " : '" << argument << "'" << "\r\n";

	for (DESCRIPTOR_DATA *i = descriptor_list; i; i = i->next)
	{
		// �������� ��� �������� ���������� ���� �� ���� ����� ����� ���� �����...
		if (STATE(i) == CON_PLAYING
			&& i->character
			&& (GET_LEVEL(i->character) < LVL_IMMORT || !strcmp(GET_NAME(i->character), "��������"))
			&& PRF_FLAGGED(i->character, PRF_OFFTOP_MODE))
		{
			if (ignores(i->character, ch, IGNORE_OFFTOP))
				continue;
			send_to_char(i->character, "%s%s%s", CCCYN(i->character, C_NRM), text.str().c_str(), CCNRM(i->character, C_NRM));
		}
	}

	char timeBuf[9];
	time_t tmp_time = time(0);
	strftime(timeBuf, sizeof(timeBuf), "[%H:%M] ", localtime(&tmp_time));
	std::string remember = timeBuf + text.str();

	if (remember_offtop.size() >= max_remember_offtop)
		remember_offtop.erase(remember_offtop.begin());
	remember_offtop.push_back(remember);

	set_wait(ch, 1, FALSE);
}

ACMD(do_remember_char)
{
	int i, j = 0, k = 0;
	char arg[MAX_INPUT_LENGTH];

	if (IS_NPC(ch))
		return;

// ���� ��� ��������� - ������ ������ �����
	if (!*argument) {
		for (i = 0; i < MAX_REMEMBER_TELLS; i++) {
			j = GET_LASTTELL(ch) + i;
			if (j >= MAX_REMEMBER_TELLS)
				j = j - MAX_REMEMBER_TELLS;
			if (GET_TELL(ch, j)[0] != '\0') {
				if (k == 0)
					send_to_char("&C", ch);
				k = 1;
				send_to_char(GET_TELL(ch, j), ch);
				send_to_char("\r\n", ch);
			}
		}

		if (!k) {
			send_to_char("��� ������ ���������.\r\n", ch);
		} else {
			send_to_char("&n", ch);
		}
		return;
	}

	argument = one_argument(argument, arg);

	if (GET_LEVEL(ch) >= LVL_IMMORT && is_abbrev(arg, "��������"))
	{
		// ������ �������� � �����
		if (!IS_IMMORTAL(ch) && !Privilege::check_flag(ch, Privilege::KRODER))
			return;
		for (i = 0; i < MAX_REMEMBER_PRAY; i++)
		{
			j = num_pray + i;
			if (j >= MAX_REMEMBER_PRAY)
				j = j - MAX_REMEMBER_PRAY;
			if (remember_pray[j][0] != '\0')
			{
				if (k == 0)
					send_to_char("&C", ch);
				k = 1;
				send_to_char(remember_pray[j], ch);
				send_to_char("\r\n", ch);
			}
		}

		if (!k)
			send_to_char("����� �� ������ �����.\r\n", ch);
		else
			send_to_char("&n", ch);
	}
	else if (is_abbrev(arg, "�������"))
	{
		// ������ ����� � ����
		for (i = 0; i < MAX_REMEMBER_GOSSIP; i++)
		{
			j = num_gossip + i;
			if (j >= MAX_REMEMBER_GOSSIP)
				j = j - MAX_REMEMBER_GOSSIP;
			if (remember_gossip[j][0] != '\0')
			{
				if (k == 0)
					send_to_char("��������� ����� � ����:\r\n", ch);
				k = 1;
				send_to_char(remember_gossip[j], ch);
				send_to_char("\r\n", ch);
			}
		}

		if (!k)
			send_to_char("����� �� ������ � ����.\r\n", ch);
		else
			send_to_char("&n", ch);
	}
	else if (GET_LEVEL(ch) < LVL_IMMORT && is_abbrev(arg, "������"))
	{
		// ������ ����� �������
		if (remember_offtop.empty())
			send_to_char("����� ���� �� ��������.\r\n", ch);
		else
		{
			send_to_char("��������� ����� � ������ ������:\r\n", ch);
			for (std::list<std::string>::const_iterator it = remember_offtop.begin(); it != remember_offtop.end(); ++it)
				send_to_char(ch, "%s%s%s", CCCYN(ch, C_NRM), (*it).c_str(), CCNRM(ch, C_NRM));
		}
	}
	else
	{
		if (IS_IMMORTAL(ch))
			send_to_char("������ �������: ��������� [��� ����������|�������|��������]\r\n", ch);
		else
			send_to_char("������ �������: ��������� [��� ����������|�������|������]\r\n", ch);
	}
}

// shapirus
void ignore_usage(CHAR_DATA * ch)
{
	send_to_char("������ �������: ������������ <���|���> <�����|���> <��������|������>\r\n"
		     "��������� ������:\r\n"
		     "  ������� �������� ������� �������� ������ �������\r\n"
		     "  ������� ����� ������ ������� ��������\r\n", ch);
}

int ign_find_id(char *name, long *id)
{
	extern struct player_index_element *player_table;
	extern int top_of_p_table;
	int i;

	for (i = 0; i <= top_of_p_table; i++) {
		if (!strcmp(name, player_table[i].name)) {
			if (player_table[i].level >= LVL_IMMORT)
				return 0;
			*id = player_table[i].id;
			return 1;
		}
	}
	return -1;
}

const char * ign_find_name(long id)
{
	extern struct player_index_element *player_table;
	extern int top_of_p_table;
	int i;

	for (i = 0; i <= top_of_p_table; i++)
		if (id == player_table[i].id)
			return player_table[i].name;
	return "���-��";
}

char *text_ignore_modes(unsigned long mode, char *buf)
{
	buf[0] = 0;

	if (IS_SET(mode, IGNORE_TELL))
		strcat(buf, " �������");
	if (IS_SET(mode, IGNORE_SAY))
		strcat(buf, " ��������");
	if (IS_SET(mode, IGNORE_WHISPER))
		strcat(buf, " �������");
	if (IS_SET(mode, IGNORE_ASK))
		strcat(buf, " ��������");
	if (IS_SET(mode, IGNORE_EMOTE))
		strcat(buf, " ������");
	if (IS_SET(mode, IGNORE_SHOUT))
		strcat(buf, " �������");
	if (IS_SET(mode, IGNORE_GOSSIP))
		strcat(buf, " �������");
	if (IS_SET(mode, IGNORE_HOLLER))
		strcat(buf, " �����");
	if (IS_SET(mode, IGNORE_GROUP))
		strcat(buf, " ������");
	if (IS_SET(mode, IGNORE_CLAN))
		strcat(buf, " �������");
	if (IS_SET(mode, IGNORE_ALLIANCE))
		strcat(buf, " ��������");
	if (IS_SET(mode, IGNORE_OFFTOP))
		strcat(buf, " ��������");
	return buf;
}

ACMD(do_ignore)
{
	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];
	char arg3[MAX_INPUT_LENGTH];
	unsigned int mode = 0, list_empty = 1, all = 0, flag = 0;
	long vict_id;
	struct ignore_data *ignore, *cur;
	char buf[256], buf1[256], name[50];

	argument = one_argument(argument, arg1);
	argument = one_argument(argument, arg2);
	argument = one_argument(argument, arg3);

// ����� ��������� -- ������� �������
	if (arg1[0] && (!arg2[0] || !arg3[0])) {
		ignore_usage(ch);
		return;
	}
// ��� ������ ��� ���������� ������� ���� ������
	if (!arg1[0] && !arg2[0] && !arg3[0]) {
		sprintf(buf, "%s�� ����������� ��������� ����������:%s\r\n", CCWHT(ch, C_NRM), CCNRM(ch, C_NRM));
		send_to_char(buf, ch);
		for (ignore = IGNORE_LIST(ch); ignore; ignore = ignore->next) {
			if (!ignore->id)
				continue;
			if (ignore->id == -1) {
				strcpy(name, "���");
			} else {
				strcpy(name, ign_find_name(ignore->id));
				name[0] = UPPER(name[0]);
			}
			sprintf(buf, "  %s: ", name);
			send_to_char(buf, ch);
			mode = ignore->mode;
			send_to_char(text_ignore_modes(mode, buf), ch);
			send_to_char("\r\n", ch);
			list_empty = 0;
		}
		if (list_empty)
			send_to_char("  ������ ����.\r\n", ch);
		return;
	}

	if (is_abbrev(arg2, "���"))
		all = 1;
	else if (is_abbrev(arg2, "�������"))
		flag = IGNORE_TELL;
	else if (is_abbrev(arg2, "��������"))
		flag = IGNORE_SAY;
	else if (is_abbrev(arg2, "�������"))
		flag = IGNORE_WHISPER;
	else if (is_abbrev(arg2, "��������"))
		flag = IGNORE_ASK;
	else if (is_abbrev(arg2, "������"))
		flag = IGNORE_EMOTE;
	else if (is_abbrev(arg2, "�������"))
		flag = IGNORE_SHOUT;
	else if (is_abbrev(arg2, "�������"))
		flag = IGNORE_GOSSIP;
	else if (is_abbrev(arg2, "�����"))
		flag = IGNORE_HOLLER;
	else if (is_abbrev(arg2, "������"))
		flag = IGNORE_GROUP;
	else if (is_abbrev(arg2, "�������"))
		flag = IGNORE_CLAN;
	else if (is_abbrev(arg2, "��������"))
		flag = IGNORE_ALLIANCE;
	else if (is_abbrev(arg2, "������"))
		flag = IGNORE_OFFTOP;
	else {
		ignore_usage(ch);
		return;
	}

// ����� "���" ������������� id -1
	if (is_abbrev(arg1, "���")) {
		vict_id = -1;
	} else {
		// ��������, ��� ����������� ��� �� ������ ������ ����������
		// � �� �� ���, � ������ ������� ��� id
		switch (ign_find_id(arg1, &vict_id)) {
		case 0:
			send_to_char("������ ������������ �����, ��� ����� ��������.\r\n", ch);
			return;
		case -1:
			send_to_char("��� ������ ���������, �������� ���.\r\n", ch);
			return;
		}
	}

// ���� ������ � ������
	for (ignore = IGNORE_LIST(ch); ignore; ignore = ignore->next) {
		if (ignore->id == vict_id)
			break;
		if (!ignore->next)
			break;
	}

	if (is_abbrev(arg3, "��������")) {
// ������� ����� ������� ������ � ������, ���� �� �����
		if (!ignore || ignore->id != vict_id) {
			CREATE(cur, struct ignore_data, 1);
			cur->next = NULL;
			if (!ignore)	// ������� ������ ����� ������, ���� ��� ���
				IGNORE_LIST(ch) = cur;
			else
				ignore->next = cur;
			ignore = cur;
			ignore->id = vict_id;
			ignore->mode = 0;
		}
		mode = ignore->mode;
		if (all) {
			SET_BIT(mode, IGNORE_TELL);
			SET_BIT(mode, IGNORE_SAY);
			SET_BIT(mode, IGNORE_WHISPER);
			SET_BIT(mode, IGNORE_ASK);
			SET_BIT(mode, IGNORE_EMOTE);
			SET_BIT(mode, IGNORE_SHOUT);
			SET_BIT(mode, IGNORE_GOSSIP);
			SET_BIT(mode, IGNORE_HOLLER);
			SET_BIT(mode, IGNORE_GROUP);
			SET_BIT(mode, IGNORE_CLAN);
			SET_BIT(mode, IGNORE_ALLIANCE);
			SET_BIT(mode, IGNORE_OFFTOP);
		} else {
			SET_BIT(mode, flag);
		}
		ignore->mode = mode;
	} else if (is_abbrev(arg3, "������")) {
		if (!ignore || ignore->id != vict_id) {
			if (vict_id == -1) {
				send_to_char("�� � ��� �� ����������� ���� �����.\r\n", ch);
			} else {
				strcpy(name, ign_find_name(vict_id));
				name[0] = UPPER(name[0]);
				sprintf(buf,
					"�� � ��� �� ����������� "
					"��������� %s%s%s.\r\n", CCWHT(ch, C_NRM), name, CCNRM(ch, C_NRM));
				send_to_char(buf, ch);
			}
			return;
		}
		mode = ignore->mode;
		if (all) {
			mode = 0;
		} else {
			REMOVE_BIT(mode, flag);
		}
		ignore->mode = mode;
		if (!mode)
			ignore->id = 0;
	} else {
		ignore_usage(ch);
		return;
	}
	if (mode) {
		if (ignore->id == -1) {
			sprintf(buf, "��� ���� ����� �� �����������:%s.\r\n", text_ignore_modes(ignore->mode, buf1));
			send_to_char(buf, ch);
		} else {
			strcpy(name, ign_find_name(ignore->id));
			name[0] = UPPER(name[0]);
			sprintf(buf, "��� ��������� %s%s%s �� �����������:%s.\r\n",
				CCWHT(ch, C_NRM), name, CCNRM(ch, C_NRM), text_ignore_modes(ignore->mode, buf1));
			send_to_char(buf, ch);
		}
	} else {
		if (vict_id == -1) {
			send_to_char("�� ������ �� ����������� ���� �����.\r\n", ch);
		} else {
			strcpy(name, ign_find_name(vict_id));
			name[0] = UPPER(name[0]);
			sprintf(buf, "�� ������ �� ����������� ��������� %s%s%s.\r\n",
				CCWHT(ch, C_NRM), name, CCNRM(ch, C_NRM));
			send_to_char(buf, ch);
		}
	}
}
