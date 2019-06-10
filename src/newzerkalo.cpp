/* ************************************************************************
*   File: newzerkalo.cpp                      Part of NewZerkalo MUD      *
*  Usage: prool subprograms for NewZerkalo MUD                            *
*                                                                         *
*  Copyleft 2011-2019, Prool                                              *
*                                                                         *
*  Author: Prool, proolix@gmail.com, http://prool.kharkov.org             *
************************************************************************ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "act.wizard.hpp"

#include "object.prototypes.hpp"
#include "world.objects.hpp"
#include "world.characters.hpp"
#include "logger.hpp"
#include "command.shutdown.hpp"
#include "obj.hpp"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "house.h"
#include "screen.h"
#include "skills.h"
#include "constants.h"
#include "olc.h"
#include "dg_scripts.h"
#include "pk.h"
#include "im.h"
#include "top.h"
#include "ban.hpp"
#include "description.h"
#include "title.hpp"
#include "names.hpp"
#include "password.hpp"
#include "privilege.hpp"
#include "depot.hpp"
#include "glory.hpp"
#include "genchar.h"
#include "file_crc.hpp"
#include "char.hpp"
#include "char_player.hpp"
#include "parcel.hpp"
#include "liquid.hpp"
#include "modify.h"
#include "room.hpp"
#include "glory_misc.hpp"
#include "glory_const.hpp"
#include "shop_ext.hpp"
#include "celebrates.hpp"
#include "player_races.hpp"
#include "birth_places.hpp"
#include "corpse.hpp"
#include "pugixml.hpp"
#include "sets_drop.hpp"
#include "fight.h"
#include "ext_money.hpp"
#include "noob.hpp"
#include "mail.h"
#include "mob_stat.hpp"
#include "char_obj_utils.inl"
#include "utils.h"
#include "structs.h"
#include "sysdep.h"
#include "conf.h"
#include "config.hpp"
#include "time_utils.hpp"
#include "global.objects.hpp"
#include "heartbeat.hpp"
#include "zone.table.hpp"

#include "newzerkalo.h"

#define BUFLEN 512

void generate_magic_enchant(OBJ_DATA *obj);

char prool_g_buf [PROOL_G_LEN];

char gluck[] = "(ptime gluck?)";

char *ptime(void)
	{
#if 0 // 0 - normal ptime() 1 - ptime disabled bikoz probable gluck
		return gluck;
#else
	char *tmstr;
	time_t mytime;

	mytime = time(0);

	tmstr = (char *) asctime(localtime(&mytime));
	*(tmstr + strlen(tmstr) - 1) = '\0';

	return tmstr;
#endif
	}

char	*to_utf(char *str)
{
	prool_g_buf[0]=0;
	koi_to_utf8(str,prool_g_buf);
	return prool_g_buf;
}

void do_dukhmada (CHAR_DATA *ch, char *argument, int/* cmd*/, int/* subcmd*/)
{
	mob_vnum number;
	mob_rnum r_num;
	char buf[BUFLEN];
	FILE *fp;
	char *cc;

if (!check_moves(ch,10))
	{
	//printf("prool debug: dukhmada: check moves\n");
	return;
	}

//printf("prool debug: dukhmada OK\n");

fp=fopen("dukhmada.cfg","r");
if (fp==0)
{
	send_to_char("Духмада ушел в отпуск\r\n", ch);
	return;
}

if (*argument==0)
	{
		while (1)
			{
			buf[0]=0;
			fgets(buf,BUFLEN,fp);
			if (buf[0]==0) break;
			if (buf[0]=='#') buf[0]=' ';
			cc=strchr(buf,'#');
			if (cc) strcpy(cc,"\r\n");
			send_to_char(buf,ch);
			}
	fclose(fp);
	return;
	}

while(1)
    {
    buf[0]=0;
    fgets(buf,BUFLEN,fp);
    if (buf[0]==0) break;
    if (buf[0]=='#') continue;
    cc=strchr(buf,'#');
    if (cc==0) continue;
    number=atoi(cc+1);
    if (number==0) continue;
    *cc=0;
    if (!strcmp(buf,argument+1))
	{
	goto l_dukh;
	}
    }
send_to_char("Духмада не знает такого предмета\r\n",ch);
fclose(fp);
return;
l_dukh:;

		if ((r_num = real_object(number)) < 0)
		{
			send_to_char("Духмада пошарил в астрале, но там оказалось пусто\r\n", ch);
			fclose(fp);
			return;
		}
		const auto obj = world_objects.create_from_prototype_by_rnum(r_num);
		obj->set_crafter_uid(GET_UNIQUE(ch));

#if 0  // prool: все это взято из do_load() и возможно не нужно
		if (number == GlobalDrop::MAGIC1_ENCHANT_VNUM
			|| number == GlobalDrop::MAGIC2_ENCHANT_VNUM
			|| number == GlobalDrop::MAGIC3_ENCHANT_VNUM)
		{
			generate_magic_enchant(obj.get());
		}
#endif

		obj_to_char(obj.get(), ch);

		act("$n достал$g из астрала $o3!", FALSE, ch, obj.get(), 0, TO_ROOM);
		act("Вы достали из астрала $o3.", FALSE, ch, obj.get(), 0, TO_CHAR);
		load_otrigger(obj.get());
		obj_decay(obj.get());
		olc_log("%s dukhmada::load obj %s #%d", GET_NAME(ch), obj->get_short_description().c_str(), number);
fclose(fp);
}

void do_accio_trup(CHAR_DATA *ch, char * /*argument*/, int/* cmd*/, int/* subcmd*/)
{
char buf[BUFLEN];
char imya_trupa[BUFLEN];
OBJ_DATA * trup;
bool found;

//send_to_char("do_accio_trup()\r\n",ch);

// поиск трупа

printf("%s prool debug: accio trup %s ", ptime(), to_utf((char *)GET_NAME(ch)));

sprintf(buf,"Боги знают, что вас зовут %s\r\n", GET_NAME(ch)); send_to_char(buf,ch);

sprintf(buf,"Боги знают, что ваш труп называется \"труп_%s\"\r\n", GET_PAD(ch,1)); send_to_char(buf,ch);

sprintf(imya_trupa,"труп_%s",GET_PAD(ch,1));

//bool print_imm_where_obj(CHAR_DATA *ch, char *arg, int num)
{
found = false;

	world_objects.foreach([&](const OBJ_DATA::shared_ptr object)	/* maybe it is possible to create some index instead of linear search */
	{
		if (isname(imya_trupa, object->get_aliases()))
		{
			found = true;
			trup=object.get();
		}
	});

}

if (found==true)
	{
		{
		if (trup->get_in_room() <= NOWHERE)
		        {
		        send_to_char("Мы нашли труп, но он находится не в какой-то комнате, а где-то еще (может, у кого-то в инвентаре)\r\nТакие трупы не мы не можем призвать!\r\n", ch);
			printf("SOMEWHERE\n");
			return;
		        }

		}
	printf("found\n");
	send_to_char("\r\nМы нашли труп и призываем его сюда\r\n",ch);
	}
else
	{
	printf("NOT found\n");
	send_to_char("Мы НЕ нашли труп\r\n",ch); return; 
	}

// obj from room
	obj_from_room(trup);
#if 0 // prool
// obj to room
	obj_to_room(trup, ch->in_room);
#endif
	obj_to_char(trup, ch);

	if (trup->get_carried_by() == ch)
	{
	act("Вы возвратили себе $o3.", FALSE, ch, trup, 0, TO_CHAR);
	act("$n возвратил$g $o3.", TRUE, ch, trup, 0, TO_ROOM);
	extract_obj(trup);
	}
	else
	{
	send_to_char("Вернуть труп не получилось!\r\n",ch);
	}
}
