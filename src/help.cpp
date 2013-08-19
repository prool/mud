// $RCSfile$     $Date$     $Revision$
// Copyright (c) 2013 Krodo
// Part of Bylins http://www.mud.ru

#include <iterator>
#include <sstream>
#include <iomanip>
#include <map>
#include <boost/format.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/range/algorithm/remove_if.hpp>
#include "help.hpp"
#include "structs.h"
#include "db.h"
#include "utils.h"
#include "modify.h"
#include "house.h"
#include "sets_drop.hpp"
#include "handler.h"
#include "screen.h"
#include "spells.h"

extern char *help;
extern const char *weapon_affects[];
extern const char *no_bits[];
extern const char *class_name[];
void index_boot(int mode);
std::string print_obj_affects(const obj_affected_type &affect);


////////////////////////////////////////////////////////////////////////////////

namespace HelpSystem
{

struct help_node
{
	help_node() : min_level(0), sets_drop_page(false) {};

	// ���� ��� ������
	std::string keyword;
	// ����� �������
	std::string entry;
	// ��������� ������� ��� ������ (�������� ����� ������ LVL_IMMORT)
	int min_level;
	// ��� ��������������� ������� ����� �����
	// �� ������ � ������ ��� ������, ������� ����� ��������� ������
	bool sets_drop_page;
};

// �������, ������������ �� ������ �� ������ (STATIC)
std::vector<help_node> static_help;
// ������� ��� �����, ��� ����� ����� ���������
// ����, ����� ������, ��������� ���� (DYNAMIC)
std::vector<help_node> dynamic_help;
// ���� ��� �������� ������������� ���������� dynamic_help �� ������� ��� � ������
bool need_update = false;

class UserSearch
{
public:
	UserSearch(CHAR_DATA *in_ch)
		: strong(false), stop(false), diff_keys(false), level(0), topic_num(0), curr_topic_num(0)
	{ ch = in_ch; };

	// ������ ���
	CHAR_DATA *ch;
	// ������� ����� (! �� �����)
    bool strong;
    // ���� ��������� ������� �� ������� �������
    bool stop;
    // ���� ������� ���� � ����� ������ ������� � key_list
    // ���� ����� 1 � ����������� ������� ������ - ���������� ������ ���� �����
    // ���� ������� ��� � ����� - ���������� ������ ���� ������
    bool diff_keys;
    // ������� ������� ��� ��������� ������ �����
    int level;
    // ����� �� �.���������_�����
    int topic_num;
    // ������� ������ ��� topic_num != 0
    int curr_topic_num;
    // ��������� �����
    std::string arg_str;
    // ������������� ������ ���������� �������
    std::vector<std::vector<help_node>::const_iterator> key_list;

	// ��������� ������ ����� search � ������ �������
    void process(int flag);
    // ���������� ��� ����� ������� � ���������� �������
    void search(const std::vector<help_node> &cont);
    // ���������� ���� ����� ������ �� �������
    void print_not_found() const;
    // ���������� ���� ����������� ������ �������
    void print_curr_topic(const help_node &node) const;
    // ���������� ���� ������ ��� ������ ����
    // � ����������� �� ��������� key_list � diff_keys
    void print_key_list() const;
};

void add(const std::string key_str, const std::string entry_str, int min_level, Flags add_flag)
{
	if (key_str.empty() || entry_str.empty())
	{
		log("SYSERROR: empty str '%s' -> '%s' (%s %s %d)",
				key_str.c_str(), entry_str.c_str(), __FILE__, __func__, __LINE__ );
		return;
	}

	help_node tmp_node;
	tmp_node.keyword = key_str;
	lower_convert(tmp_node.keyword);
	tmp_node.entry = entry_str;
	tmp_node.min_level = min_level;

	switch(add_flag)
	{
	case STATIC:
		static_help.push_back(tmp_node);
		break;
	case DYNAMIC:
		dynamic_help.push_back(tmp_node);
		break;
	default:
		log("SYSERROR: wrong add_flag = %d (%s %s %d)",
				add_flag, __FILE__, __func__, __LINE__ );
	};
}

void add_sets(const std::string key_str, const std::string entry_str)
{
	if (key_str.empty() || entry_str.empty())
	{
		log("SYSERROR: empty str '%s' -> '%s' (%s %s %d)",
				key_str.c_str(), entry_str.c_str(), __FILE__, __func__, __LINE__ );
		return;
	}

	help_node tmp_node;
	tmp_node.keyword = key_str;
	lower_convert(tmp_node.keyword);
	tmp_node.entry = entry_str;
	tmp_node.sets_drop_page = true;

	dynamic_help.push_back(tmp_node);
}

void init_group_zones()
{
	std::stringstream out;
	for (int rnum = 0, i = 1; rnum <= top_of_zone_table; ++rnum)
	{
		const int group = zone_table[rnum].group;
		if (group > 1)
		{
			out << boost::format("  %2d - %s (��. %d+).\r\n") % i % zone_table[rnum].name % group;
			++i;
		}
	}
	add("�������������", out.str(), 0, DYNAMIC);
}

////////////////////////////////////////////////////////////////////////////////
namespace PrintActivators
{

// ��������� ������ ��� ����� �����
struct clss_activ_node
{
	clss_activ_node() { total_affects = clear_flags; };
	// �������
	FLAG_DATA total_affects;
	// ��������
	std::vector<obj_affected_type> affected;
	// �����
	std::map<int, int> skills;
};

// ���������� �������
struct dup_node
{
	// ������ ���������
	std::string clss;
	// ��� �������
	std::string afct;
};

// ���������� ������� aff � ������ l � ��������� �� ������������
// � ������ ������ ���������� ������������
void add_affected(std::vector<obj_affected_type> &l, const obj_affected_type &aff)
{
	if (aff.modifier != 0)
	{
		std::vector<obj_affected_type>::iterator k =
			std::find_if(l.begin(), l.end(),
				boost::bind(std::equal_to<int>(),
					boost::bind(&obj_affected_type::location, _1), aff.location));
		if (k != l.end())
		{
			k->modifier += aff.modifier;
		}
		else
		{
			l.push_back(aff);
		}
	}
}

// ����������� ��� ���������� ����� add_affected
void sum_affected(std::vector<obj_affected_type> &l, const boost::array<obj_affected_type, MAX_OBJ_AFFECT> &r)
{
	for (int i = 0; i < MAX_OBJ_AFFECT; ++i)
	{
		add_affected(l, r[i]);
	}
}

// ����������� ��� ���������� ����� add_affected
void sum_affected(std::vector<obj_affected_type> &l, const std::vector<obj_affected_type> &r)
{
	for (std::vector<obj_affected_type>::const_iterator it = r.begin(),
		itend = r.end(); it != itend; ++it)
	{
		add_affected(l, *it);
	}
}

void sum_skills(std::map<int, int> &target, const std::map<int, int> &add)
{
	for (std::map<int, int>::const_iterator i = add.begin(),
		iend = add.end(); i != iend; ++i)
	{
		if (i->second != 0)
		{
			std::map<int, int>::iterator ii = target.find(i->first);
			if (ii != target.end())
			{
				ii->second += i->second;
			}
			else
			{
				target[i->first] = i->second;
			}
		}
	}
}

void sum_skills(std::map<int, int> &target, const OBJ_DATA *obj)
{
	if (obj->has_skills())
	{
		std::map<int, int> tmp_skills;
		obj->get_skills(tmp_skills);
		sum_skills(target, tmp_skills);
	}
}

// �������� �������� flag_data_by_num()
bool check_num_in_unique_bit_flag_data(const unique_bit_flag_data &data, const int num)
{
	return num < 0   ? false :
		   num < 30  ? *(data.flags) & (1 << num) :
		   num < 60  ? *(data.flags + 1) & (1 << (num - 30)) :
		   num < 90  ? *(data.flags + 2) & (1 << (num - 60)) :
		   num < 120 ? *(data.flags + 3) & (1 << (num - 90)) : false;
}

// ���������� ������� ������ � " + " ����� ������������
std::string print_skills(const std::map<int, int> &skills, bool activ)
{
	std::string skills_str;
	for (std::map<int, int>::const_iterator i = skills.begin(), iend = skills.end();
		i != iend; ++i)
	{
		if (i->second != 0)
		{
			skills_str += boost::str(boost::format("%s%s%s%s%s%s%d%%%s\r\n")
					% (activ ? " +    " : "   ") % KCYN %  skill_info[i->first].name % KNRM
					% KCYN % (i->second < 0 ? " �������� �� " : " �������� �� ")
					% abs(i->second) % KNRM);
		}
	}

	if (!skills_str.empty())
	{
		std::string head = activ ? " + " : "   ";
		return head + "������ ������ :\r\n" + skills_str;
	}

	return skills_str;
}

// ���������� ������ � ��������� ����� ������ ���� ��������
std::string print_obj_affects(const OBJ_DATA * const obj)
{
	std::stringstream out;

	out << GET_OBJ_PNAME(obj, 0) << "\r\n";

	if (sprintbits(obj->obj_flags.no_flag, no_bits, buf2, ","))
	{
		out << "���������� : " << buf2 << "\r\n";
	}

	if (GET_OBJ_TYPE(obj) == ITEM_WEAPON)
	{
		const int drndice = GET_OBJ_VAL(obj, 1);
		const int drsdice = GET_OBJ_VAL(obj, 2);
		out << boost::format("��������� ����������� '%dD%d' ������� %.1f\r\n")
				% drndice % drsdice % ((drsdice + 1) * drndice / 2.0);
	}

	if (GET_OBJ_TYPE(obj) == ITEM_WEAPON
		|| CAN_WEAR(obj, ITEM_WEAR_SHIELD)
		|| CAN_WEAR(obj, ITEM_WEAR_HANDS))
	{
		out << "��� : " << GET_OBJ_WEIGHT(obj) << "\r\n";
	}

	if (sprintbits(GET_OBJ_AFFECTS(obj), weapon_affects, buf2, ","))
	{
		out << "������� : " << buf2 << "\r\n";
	}

	std::string tmp_str;
	for (int i = 0; i < MAX_OBJ_AFFECT; i++)
	{
		if (obj->affected[i].modifier != 0)
		{
			tmp_str += print_obj_affects(obj->affected[i]);
		}
	}
	if (!tmp_str.empty())
	{
		out << "�������� :\r\n" << tmp_str;
	}

	if (obj->has_skills())
	{
		std::map<int, int> skills;
		obj->get_skills(skills);
		out << print_skills(skills, false);
	}

	return out.str();
}

// ���������� ����������� ���������� ��������
std::string print_activator(class_to_act_map::const_iterator &activ, const OBJ_DATA * const obj)
{
	std::stringstream out;

	out << " + ��������� :";
	for (int i = 0; i <= NUM_CLASSES * NUM_KIN; ++i)
	{
		if (check_num_in_unique_bit_flag_data(activ->first, i))
		{
			if (i < NUM_CLASSES * NUM_KIN)
			{
				out << " " << class_name[i];
			}
			else
			{
				out << " �������";
			}
		}
	}
	out << "\r\n";

	flag_data affects = activ->second.get_affects();
	if (sprintbits(affects, weapon_affects, buf2, ","))
	{
		out << " + ������� : " << buf2 << "\r\n";
	}

	boost::array<obj_affected_type, MAX_OBJ_AFFECT> affected = activ->second.get_affected();
	std::string tmp_str;
	for (int i = 0; i < MAX_OBJ_AFFECT; i++)
	{
		if (affected[i].modifier != 0)
		{
			tmp_str += " + " + print_obj_affects(affected[i]);
		}
	}
	if (!tmp_str.empty())
	{
		out << " + �������� :\r\n" << tmp_str;
	}

	if (GET_OBJ_TYPE(obj) == ITEM_WEAPON)
	{
		int drndice = 0, drsdice = 0;
		activ->second.get_dices(drsdice, drndice);
		if (drsdice > 0 && drndice > 0)
		{
			out << boost::format(" + ������������� ��������� ����������� '%dD%d' ������� %.1f\r\n")
					% drndice % drsdice % ((drsdice + 1) * drndice / 2.0);
		}
	}

	const int weight = activ->second.get_weight();
	if (weight > 0)
	{
		out << " + ������������� ���: " << weight << "\r\n";
	}

	if (activ->second.has_skills())
	{
		std::map<int, int> skills;
		activ->second.get_skills(skills);
		out << print_skills(skills, true);
	}

	return out.str();
}

////////////////////////////////////////////////////////////////////////////////
struct activators_obj
{
	activators_obj()
	{
		native_no_flag = clear_flags;
		native_affects = clear_flags;
	};

	// ����� ����� � �� ��������� ������
	std::map<int, clss_activ_node> clss_list;
	// ��������� ����� ������
	FLAG_DATA native_no_flag;
	FLAG_DATA native_affects;
	std::vector<obj_affected_type> native_affected;
	std::map<int, int> native_skills;

	// ���������� ������� clss_list �������� ����
	void fill_class(set_info::const_iterator k);
	// ������ �� ����������� ���� ��������� � ������� ���� �� clss_list
	void fill_node(const set_info &set);
	// ���������� clss_list �� �������� ���������� ������� ����������� � ������ ����
	std::string print();
};

void activators_obj::fill_class(set_info::const_iterator k)
{
	for (qty_to_camap_map::const_iterator m = k->second.begin(),
		mend = k->second.end(); m != mend; ++m)
	{
		for (class_to_act_map::const_iterator q = m->second.begin(),
			qend = m->second.end(); q != qend; ++q)
		{
			for (int i = 0; i <= NUM_CLASSES * NUM_KIN; ++i)
			{
				if (check_num_in_unique_bit_flag_data(q->first, i))
				{
					struct clss_activ_node tmp_node;
					clss_list[i] = tmp_node;
				}
			}
		}
	}
}

void activators_obj::fill_node(const set_info &set)
{
	for (set_info::const_iterator k = set.begin(),
		kend = set.end(); k != kend; ++k)
	{
		// ���������� ���������� ����� �����
		for (std::map<int, clss_activ_node>::iterator w = clss_list.begin(),
			wend = clss_list.end(); w != wend; ++w)
		{
			// ���� �� ���-�� ����������� � ����� �� �������������
			for (qty_to_camap_map::const_reverse_iterator m = k->second.rbegin(),
				mend = k->second.rend(); m != mend; ++m)
			{
				bool found = false;
				// �� ������� ���������� �� �����
				for (class_to_act_map::const_iterator q = m->second.begin(),
					qend = m->second.end(); q != qend; ++q)
				{
					if (check_num_in_unique_bit_flag_data(q->first, w->first))
					{
						// ������������ ����������� ��� ������ �����
						w->second.total_affects += q->second.get_affects();
						sum_affected(w->second.affected, q->second.get_affected());
						// �����
						std::map<int, int> tmp_skills;
						q->second.get_skills(tmp_skills);
						sum_skills(w->second.skills, tmp_skills);
						found = true;
						break;
					}
				}
				if (found)
				{
					break;
				}
			}
		}
	}
}

std::string activators_obj::print()
{
	std::vector<dup_node> dup_list;

	for (std::map<int, clss_activ_node>::iterator cls_it = clss_list.begin(),
		cls_it_end = clss_list.end(); cls_it != cls_it_end;  ++cls_it)
	{
		// ���������� �������� ������ �����
		dup_node node;
		node.clss += cls_it->first < NUM_CLASSES * NUM_KIN ? class_name[cls_it->first] : "�������";
		// affects
		cls_it->second.total_affects += native_affects;
		if (sprintbits(cls_it->second.total_affects, weapon_affects, buf2, ","))
		{
			node.afct += " + ������� : " + std::string(buf2) + "\r\n";
		}
		// affected
		sum_affected(cls_it->second.affected, native_affected);
		// ���������� ��� ����� �������� ��������� ������ �� ����������
		std::sort(cls_it->second.affected.begin(), cls_it->second.affected.end(),
			boost::bind(std::less<int>(),
				boost::bind(&obj_affected_type::location, _1),
				boost::bind(&obj_affected_type::location, _2)));
		std::string tmp_str;
		for (std::vector<obj_affected_type>::const_iterator i = cls_it->second.affected.begin(),
			iend = cls_it->second.affected.end(); i != iend; ++i)
		{
			tmp_str += " + " + print_obj_affects(*i);
		}
		if (!tmp_str.empty())
		{
			node.afct += " + �������� :\r\n" + tmp_str;
		}
		// �����
		sum_skills(cls_it->second.skills, native_skills);
		node.afct += print_skills(cls_it->second.skills, true);

		// ������� ���������� �� �������� ����
		std::vector<dup_node>::iterator i =
			std::find_if(dup_list.begin(), dup_list.end(),
				boost::bind(std::equal_to<std::string>(),
					boost::bind(&dup_node::afct, _1), node.afct));
		if (i != dup_list.end())
		{
			i->clss += ", " + node.clss;
		}
		else
		{
			dup_list.push_back(node);
		}
	}

	std::string out_str;
	for (std::vector<dup_node>::const_iterator i = dup_list.begin(),
		iend = dup_list.end(); i != iend; ++i)
	{
		out_str += "��������� : " + i->clss + "\r\n" + i->afct;
	}
	return out_str;
}
// activators_obj
////////////////////////////////////////////////////////////////////////////////

std::string print_fullset_stats(const set_info &set)
{
	std::stringstream out;
	activators_obj activ;

	// ������ ������ - ������ ����� ��������� + ���� ���� � clss_list
	for (set_info::const_iterator k = set.begin(),
		kend = set.end(); k != kend; ++k)
	{
		const int rnum = real_object(k->first);
		if (rnum < 0)
		{
			continue;
		}
		const OBJ_DATA * const obj = obj_proto[rnum];

		// ��������� ������ ����� �� ������
		activ.native_no_flag += GET_OBJ_NO(obj);
		activ.native_affects += GET_OBJ_AFFECTS(obj);
		sum_affected(activ.native_affected, obj->affected);
		sum_skills(activ.native_skills, obj);

		// ���� �����
		activ.fill_class(k);
	}

	// ���� ���������� �� ������
	activ.fill_node(set);

	// �������� ���, ��� ����������
	out << "��������� �������� ������: \r\n";

	if (sprintbits(activ.native_no_flag, no_bits, buf2, ","))
	{
		out << "���������� : " << buf2 << "\r\n";
	}

	out << activ.print();

	return out.str();
}

// ��������� ���������� ������� �� �����������
void process()
{
	for (id_to_set_info_map::const_iterator it = obj_data::set_table.begin(),
		iend = obj_data::set_table.end(); it != iend; ++it)
	{
		std::stringstream out;
		// it->first = int_id, it->second = set_info
		out << "---------------------------------------------------------------------------\r\n";
		out << it->second.get_name() << "\r\n";
		out << "---------------------------------------------------------------------------\r\n";
		out << print_fullset_stats(it->second);
		for (set_info::const_iterator k = it->second.begin(), kend = it->second.end(); k != kend; ++k)
		{
			out << "---------------------------------------------------------------------------\r\n";
			// k->first = int_obj_vnum, k->second = qty_to_camap_map
			const int rnum = real_object(k->first);
			if (rnum < 0)
			{
				log("SYSERROR: wrong obj vnum: %d (%s %s %d)", k->first, __FILE__, __func__, __LINE__);
				continue;
			}

			const OBJ_DATA * const obj = obj_proto[rnum];
			out << print_obj_affects(obj);

			for (qty_to_camap_map::const_iterator m = k->second.begin(); m != k->second.end(); ++m)
			{
				// m->first = num_activators, m->second = class_to_act_map
				for (class_to_act_map::const_iterator q = m->second.begin(); q != m->second.end(); ++q)
				{
					out << "��������� ��� ���������: " << m->first << "\r\n";
					out << print_activator(q, obj);
				}
			}
		}
		// ��������� ������� ��� �������
		std::string set_name = "�����";
		if (it->second.get_alias().empty())
		{
			set_name += it->second.get_name();
			set_name.erase(boost::remove_if(set_name, boost::is_any_of(" ,.")), set_name.end());
			add(set_name, out.str(), 0, STATIC);
		}
		else
		{
			std::string alias = it->second.get_alias();
			std::vector<std::string> str_list;
			boost::split(str_list, alias, boost::is_any_of(","));
			for (std::vector<std::string>::iterator k = str_list.begin(),
				kend = str_list.end(); k != kend; ++k)
			{
				k->erase(boost::remove_if(*k, boost::is_any_of(" ,.")), k->end());
				add(set_name + "���" + *k, out.str(), 0, STATIC);
			}
		}
	}
}

} // namespace PrintActivators
////////////////////////////////////////////////////////////////////////////////

void check_update_dynamic()
{
	if (need_update)
	{
		need_update = false;
		reload(DYNAMIC);
	}
}

void reload(Flags flag)
{
	switch(flag)
	{
	case STATIC:
		static_help.clear();
		index_boot(DB_BOOT_HLP);
		PrintActivators::process();
		// �������� ���������� ������� ����� ��������� < ��� ��������� ������
		std::sort(static_help.begin(), static_help.end(),
			boost::bind(std::less<std::string>(),
				boost::bind(&help_node::keyword, _1),
				boost::bind(&help_node::keyword, _2)));
		break;
	case DYNAMIC:
		dynamic_help.clear();
		SetsDrop::init_xhelp();
		SetsDrop::init_xhelp_full();
		ClanSystem::init_xhelp();
		init_group_zones();
		std::sort(dynamic_help.begin(), dynamic_help.end(),
			boost::bind(std::less<std::string>(),
				boost::bind(&help_node::keyword, _1),
				boost::bind(&help_node::keyword, _2)));
		break;
	default:
		log("SYSERROR: wrong flag = %d (%s %s %d)", flag, __FILE__, __func__, __LINE__ );
	};
}

void reload_all()
{
	reload(STATIC);
	reload(DYNAMIC);
}

bool help_compare(const std::string &arg, const std::string &text, bool strong)
{
	if (strong)
	{
		return arg == text;
	}

	return isname(arg, text.c_str());
}

void UserSearch::process(int flag)
{
	switch(flag)
	{
	case STATIC:
		search(static_help);
		break;
	case DYNAMIC:
		search(dynamic_help);
		break;
	default:
		log("SYSERROR: wrong flag = %d (%s %s %d)", flag, __FILE__, __func__, __LINE__ );
	};
}

void UserSearch::print_not_found() const
{
	snprintf(buf, sizeof(buf), "%s uses command HELP: %s (not found)", GET_NAME(ch), arg_str.c_str());
	mudlog(buf, LGH, LVL_IMMORT, SYSLOG, TRUE);
	snprintf(buf, sizeof(buf),
			"&W�� ������ ������� '&w%s&W' ������ �� ���� �������.&n\r\n"
			"\r\n&c����������:&n\r\n���� ��������� ������� \"�������\" ��� ����������, ����� ���������� �������� �������,\r\n�������� ����������� ��������. ����� ���� ������� ������������ � �������� &C�������&n.\r\n\r\n���������� ������� ��������� ������������ � ������� ���������� �������� � ������������ ������� �����.\r\n\r\n&c�������:&n\r\n\t\"������� 3.������\"\r\n\t\"������� 4.������\"\r\n\t\"������� ������������\"\r\n\t\"������� ������!\"\r\n\t\"������� 3.������!\"\r\n\r\n��. �����: &C��������������������&n\r\n",
			arg_str.c_str());
	send_to_char(buf, ch);
}

void UserSearch::print_curr_topic(const help_node &node) const
{
	if (node.sets_drop_page)
	{
		// ���������� ������� �� ���������� ������� ������� ����� �����
		SetsDrop::print_timer_str(ch);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%s uses command HELP: %s (read)", GET_NAME(ch), arg_str.c_str());
		mudlog(buf, LGH, LVL_IMMORT, SYSLOG, TRUE);
	}
	page_string(ch->desc, node.entry);
}

void UserSearch::print_key_list() const
{
	// ���������� ������ �������
	// ���������� ���� ������� ����� ���� ���������
	// ��� ��� ��������� ���� ������� ������ ������ � ������� �������
	if (key_list.size() > 0 && (!diff_keys || key_list.size() == 1))
	{
		print_curr_topic(*(key_list[0]));
		return;
	}
	// ������ ��������� �������
	std::stringstream out;
	out << "&W�� ������ ������� '&w" << arg_str << "&W' ������� ��������� ������� �������:&n\r\n\r\n";
	for (unsigned i = 0, count = 1; i < key_list.size(); ++i, ++count)
	{
		out << boost::format("|&C %-23.23s &n|") % key_list[i]->keyword;
		if ((count % 3) == 0)
		{
			out << "\r\n";
		}
	}
	out << "\r\n";

	snprintf(buf, sizeof(buf), "%s uses command HELP: %s (list)", GET_NAME(ch), arg_str.c_str());
	mudlog(buf, LGH, LVL_IMMORT, SYSLOG, TRUE);
	page_string(ch->desc, out.str());
}

void UserSearch::search(const std::vector<help_node> &cont)
{
	// ����� � ������������� �� ������ ������� ����� lower_bound
	std::vector<help_node>::const_iterator i =
		std::lower_bound(cont.begin(), cont.end(), arg_str,
			boost::bind(std::less<std::string>(),
				boost::bind(&help_node::keyword, _1), _2));

	while (i != cont.end())
	{
		// �������� �������� ��� � ������ ����� ���������
		if (!help_compare(arg_str, i->keyword, strong))
		{
			return;
		}
		// ������� ������ (��������� ��� ������ �������)
		if (level < i->min_level)
		{
			++i;
			continue;
		}
		// key_list ����������� � ����� ������, ���� �����
		// ���� ��� ������� topic_num, ������������ �����������
		// ��� ����������� �������� ���������� ����� diff_keys
		for (unsigned k = 0; k < key_list.size(); ++k)
		{
			if (key_list[k]->entry != i->entry)
			{
				diff_keys = true;
				break;
			}
		}

		if (!topic_num)
		{
			key_list.push_back(i);
		}
		else
		{
			++curr_topic_num;
			// ����� ������ ���� �.������
			if (curr_topic_num == topic_num)
			{
				print_curr_topic(*i);
				stop = true;
				return;
			}
		}
		++i;
	}
}

} // namespace HelpSystem
////////////////////////////////////////////////////////////////////////////////

using namespace HelpSystem;

ACMD(do_help)
{
	if (!ch->desc)
	{
		return;
	}

	skip_spaces(&argument);

	// �������� ����� ������� ���� ��� ����������
	if (!*argument)
	{
		page_string(ch->desc, help, 0);
		return;
	}

	UserSearch user_search(ch);
	// trust_level ������� ��� ��������� - LVL_IMMORT
	user_search.level = GET_GOD_FLAG(ch, GF_DEMIGOD) ? LVL_IMMORT : GET_LEVEL(ch);
	// ������ �������� ��� ��������, ������ � ������ �������
	one_argument(argument, arg);
	// �������� topic_num ��� ���������� ������
	sscanf(arg, "%d.%s", &user_search.topic_num, arg);
	// ���� ��������� ������ ��������� '!' -- �������� ������� �����
	if (strlen(arg) > 1 && *(arg + strlen(arg) - 1) == '!')
	{
		user_search.strong = true;
		*(arg + strlen(arg) - 1) = '\0';
	}
	user_search.arg_str = arg;

	// ����� �� ���� �������� ��� �� ����� �� �����
	for (int i = STATIC; i < TOTAL_NUM && !user_search.stop; ++i)
	{
		user_search.process(i);
	}
	// ���� ����� �� �����, �� ���� ��� ��� ���������� ��������� ����� ���� �.������
	// ����� ������������� ���� ���-�� � ����������� �� ��������� key_list
	if (!user_search.stop)
	{
		if (user_search.key_list.empty())
		{
			user_search.print_not_found();
		}
		else
		{
			user_search.print_key_list();
		}
	}
}