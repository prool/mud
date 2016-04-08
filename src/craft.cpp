/**
 * \file Contains implementation of craft model for Bylins MUD.
 * \date 2015/12/28
 * \author Anton Gorev <kvirund@gmail.com>
 */

#include "craft.hpp"

#include "skills.h"
#include "comm.h"
#include "db.h"
#include "pugixml.hpp"

#include <boost/filesystem.hpp>

#include <iostream>
#include <string>
#include <sstream>

namespace craft
{
	const char* suffix(const size_t number)
	{
		return 1 == number % 10
			? "st"
			: (2 == number % 10
				? "nd"
				: (3 == number % 10
					? "rd"
					: "th"));
	}

	bool load()
	{
		CCraftModel model;

		return model.load();
	}
	
	void do_craft(CHAR_DATA *ch, char* /*argument*/, int /*cmd*/, int/* subcmd*/)
	{
		send_to_char("&WCrafting...&n\r\n", ch);
	}

	const char* BODY_PREFIX = "| ";
	const char* END_PREFIX = "> ";

	void CLogger::operator()(const char* format, ...)
	{
		const size_t BUFFER_SIZE = 4096;

		va_list args;
		va_start(args, format);
		char buffer[BUFFER_SIZE];
		char* p = buffer;
		size_t free_space = BUFFER_SIZE;

		std::string prefix;
		for (const auto& part : m_prefix)
		{
			prefix += part;
		}
		const size_t plength = std::min(BUFFER_SIZE, prefix.length());
		strncpy(p, prefix.c_str(), plength);
		free_space -= plength;
		p += plength;

		const size_t length = vsnprintf(p, free_space, format, args);
		va_end(args);

		if (free_space <= length)
		{
			const char truncated[] = " ...<TRUNCATED>\n";
			strncpy(buffer + BUFFER_SIZE - sizeof(truncated), truncated, sizeof(truncated));
		}

		// Use the following line to redirect craft log into syslog:
		// ::log("%s", buffer);
		// instead of output just onto console:
		// FROM HERE...
		if (syslog_converter)
		{
			syslog_converter(buffer, static_cast<int>(length));
		}

		std::cerr << buffer;
		// ...TO HERE
	}

	const std::string CCraftModel::FILE_NAME = LIB_MISC_CRAFT "index.xml";

	bool CCases::load(const pugi::xml_node* node)
	{
		for (int c = 0; c < CASES_COUNT; ++c)
		{
			const std::string node_name = std::string("case") + std::to_string(1 + c);
			const pugi::xml_node case_node = node->child(node_name.c_str());
			if (!case_node)
			{
				log("ERROR: Could not find case '%s'.\n", node_name.c_str());
				return false;
			}
			m_cases[c] = case_node.child_value();
		}

		const pugi::xml_node aliases_node = node->child("aliases");
		if (aliases_node)
		{
			for (const pugi::xml_node alias_node : aliases_node.children("alias"))
			{
				const char* value = alias_node.child_value();
				m_aliases.push_back(value);
			}
		}

		return true;
	}

	class CLoadHelper
	{
	private:
		template <typename EnumType>
		static void set_bit(FLAG_DATA& flags, const EnumType flag) { flags.set(flag); }
		template <typename EnumType>
		static void set_bit(uint32_t& flags, const EnumType flag) { SET_BIT(flags, flag); }

	public:
		template <class TFlags, typename TSuccess, typename TFail, typename TFlagsStorage>
		static void load_flags(TFlagsStorage& flags, const pugi::xml_node& root, const char* node_name, const char* node_flag,
			TSuccess success, TFail fail)
		{
			const auto node = root.child(node_name);
			if (node)
			{
				for (const auto flag : node.children(node_flag))
				{
					const char* flag_value = flag.child_value();
					try
					{
						auto value = ITEM_BY_NAME<TFlags>(flag_value);
						set_bit(flags, value);
						success(value);
					}
					catch (...)
					{
						fail(flag_value);
					}
				}
			}
		}

		enum ELoadFlagResult
		{
			ELFR_SUCCESS,
			ELFR_NO_VALUE,
			ELFR_FAIL
		};

		template <class TFlag, typename TSuccess, typename TFail, typename TNoValue>
		static ELoadFlagResult load_flag(const pugi::xml_node& root, const char* node_name, TSuccess success, TFail fail, TNoValue no_value)
		{
			const auto node = root.child(node_name);
			if (node)
			{
				const char* value = node.child_value();
				try
				{
					const TFlag type = ITEM_BY_NAME<TFlag>(value);
					success(type);
				}
				catch (...)
				{
					fail(value);
					return ELFR_FAIL;
				}
			}
			else
			{
				no_value();
				return ELFR_NO_VALUE;
			}

			return ELFR_SUCCESS;
		}

		template <typename TCatchHandler>
		static void load_integer(const char* input, int& output, TCatchHandler catch_handler)
		{
			try
			{
				output = std::stoi(input);
			}
			catch (...)
			{
				catch_handler();
			}
		}

		template <typename TSetHandler, typename TCatchHandler>
		static void load_integer(const char* input, TSetHandler set_handler, TCatchHandler catch_handler)
		{
			try
			{
				set_handler(std::stoi(input));
			}
			catch (...)
			{
				catch_handler();
			}
		}
	};

	bool CPrototype::load(const pugi::xml_node* node)
	{
		log("Loading prototype with VNUM %d...\n", m_vnum);
		CLogger::CPrefix prefix(log, BODY_PREFIX);

		const auto description = node->child("description");
		if (description)
		{
			// these fields are optional for prototypes
			m_short_desc = description.child_value("short");
			m_long_desc = description.child_value("long");
			m_keyword = description.child_value("keyword");
			m_extended_desc = description.child_value("extended");
		}

		const auto item = node->child("item");
		if (!item)
		{
			log("ERROR: The prototype with VNUM %d does not contain required \"item\" tag.\n", m_vnum);
			return false;
		}

		if (!m_cases.load(&item))
		{
			log("ERROR: could not load item cases for the prototype with VNUM %d.\n", m_vnum);
			return false;
		}

		const auto cost = node->child("cost");
		int cost_value = -1;
		if (cost)
		{
			CLoadHelper::load_integer(cost.child_value(), cost_value, [&]() { /* just do nothing: keep default value */});
		}
		else
		{
			log("WARNING: Could not find \"cost\" tag for the prototype with VNUM %d.\n", m_vnum);
		}

		if (0 > cost_value)
		{
			log("WARNING: Wrong \"cost\" value of the prototype with VNUM %d. Setting to the default value %d.\n",
				m_vnum, OBJ_DATA::DEFAULT_COST);
			cost_value = OBJ_DATA::DEFAULT_COST;
		}
		m_cost = cost_value;

		const auto rent = node->child("rent");
		int rent_on_value = -1;
		int rent_off_value = -1;
		if (rent)
		{
			const auto rent_on = rent.child("on");
			if (!rent_on)
			{
				log("WARNING: Could not find \"on\" tag for prototype with VNUM %d.\n", m_vnum);
			}
			else
			{
				CLoadHelper::load_integer(rent_on.child_value(), rent_on_value,
					[&]() { log("WARNING: Wrong value \"%s\" of the \"rent\"/\"on\" tag for prototype with VNUM %d.\n",
						rent_on.child_value(), m_vnum); });
			}

			const auto rent_off = rent.child("off");
			if (!rent_off)
			{
				log("WARNING: Could not find \"off\" tag for prototype with VNUM %d.\n", m_vnum);
			}
			else
			{
				CLoadHelper::load_integer(rent_off.child_value(), rent_off_value,
					[&]() { log("WARNING: Wrong value \"%s\" of the \"rent\"/\"off\" tag for prototype with VNUM %d.\n",
						rent_off.child_value(), m_vnum); });
			}
		}
		else
		{
			log("WARNING: Could not find \"rent\" tag for the prototype with VNUM %d.\n", m_vnum);
		}

		if (0 > rent_on_value)
		{
			log("WARNING: Wrong \"rent/on\" value of the prototype with VNUM %d. Setting to the default value %d.\n",
				m_vnum, OBJ_DATA::DEFAULT_RENT_ON);
			rent_on_value = OBJ_DATA::DEFAULT_RENT_ON;
		}
		m_rent_on = rent_on_value;

		if (0 > rent_off_value)
		{
			log("WARNING: Wrong \"rent/off\" value of the prototype with VNUM %d. Setting to the default value %d.\n",
				m_vnum, OBJ_DATA::DEFAULT_RENT_OFF);
			rent_off_value = OBJ_DATA::DEFAULT_RENT_OFF;
		}
		m_rent_off = rent_off_value;

		const auto global_maximum = node->child("global_maximum");
		if (global_maximum)
		{
			int global_maximum_value = OBJ_DATA::DEFAULT_GLOBAL_MAXIMUM;
			CLoadHelper::load_integer(global_maximum.child_value(), global_maximum_value,
				[&]() { log("WARNING: \"global_maximum\" value of the prototype with VNUM %d is not valid integer. Setting to the default value %d.\n",
					m_vnum, global_maximum_value); });

			if (0 >= global_maximum_value)
			{
				log("WARNING: Wrong \"global_maximum\" value %d of the prototype with VNUM %d. Setting to the default value %d.\n",
					global_maximum, m_vnum, OBJ_DATA::DEFAULT_GLOBAL_MAXIMUM);
				global_maximum_value = OBJ_DATA::DEFAULT_GLOBAL_MAXIMUM;
			}

			m_global_maximum = global_maximum_value;
		}

		const auto minimum_remorts = node->child("minimal_remorts");
		if (minimum_remorts)
		{
			int minimum_remorts_value = OBJ_DATA::DEFAULT_MINIMUM_REMORTS; 
			CLoadHelper::load_integer(minimum_remorts.child_value(), minimum_remorts_value,
				[&]() { log("WARNING: \"minimal_remorts\" value of the prototype with VNUM %d is not valid integer. Setting to the default value %d.\n",
					m_vnum, minimum_remorts_value); });

			if (0 > minimum_remorts_value)
			{
				log("WARNING: Wrong \"minimal_remorts\" value %d of the prototype with VNUM %d. Setting to the default value %d.\n",
					minimum_remorts_value, m_vnum, OBJ_DATA::DEFAULT_MINIMUM_REMORTS);
				minimum_remorts_value = OBJ_DATA::DEFAULT_MINIMUM_REMORTS;
			}
			m_minimum_remorts = minimum_remorts_value;
		}

		CLoadHelper::ELoadFlagResult load_result = CLoadHelper::load_flag<type_t>(*node, "type",
			[&](const auto type) { this->set_type(type); },
			[&](const auto name) { log("WARNING: Failed to set object type '%s' for prototype with VNUM %d. Prototype will be skipped.\n", name, m_vnum); },
			[&]() { log("WARNING: \"type\" tag not found for prototype with VNUM %d not found. Setting to default value: %s.\n", m_vnum, NAME_BY_ITEM(get_type()).c_str()); });
		if (CLoadHelper::ELFR_FAIL == load_result)
		{
			return false;
		}

		const auto durability = node->child("durability");
		if (durability)
		{
			const auto maximum = durability.child("maximum");
			if (maximum)
			{
				CLoadHelper::load_integer(maximum.child_value(),
					[&](const auto value) { m_maximum_durability = std::max(value, 0); },
					[&]() { log("WARNING: Wrong integer value of tag \"maximum_durability\" for prototype with VNUM %d. Leaving default value %d\n",
						m_vnum, m_maximum_durability); });
			}

			const auto current = durability.child("current");
			if (current)
			{
				CLoadHelper::load_integer(current.child_value(),
					[&](const auto value) { m_current_durability = std::min(std::max(value, 0), m_maximum_durability); },
					[&]() {
					log("WARNING: Wrong integer value of tag \"current_durability\" for prototype with VNUM %d. Setting to value of \"maximum_durability\" %d\n",
						m_vnum, m_maximum_durability);
					m_current_durability = m_maximum_durability;
				});
			}
		}

		load_result = CLoadHelper::load_flag<decltype(m_sex)>(*node, "sex",
			[&](const auto sex) { m_sex = sex; },
			[&](const auto name) { log("WARNING: Failed to set sex '%s' for prototype with VNUM %d. Prototype will be skipped.\n", name, m_vnum); },
			[&]() { log("WARNING: \"sex\" tag for prototype with VNUM %d not found. Setting to default value: %s.\n", m_vnum, NAME_BY_ITEM(m_sex).c_str()); });
		if (CLoadHelper::ELFR_FAIL == load_result)
		{
			return false;
		}

		const auto level = node->child("level");
		if (level)
		{
			CLoadHelper::load_integer(level.child_value(),
				[&](const auto value) { m_level = std::max(value, 0); },
				[&]() { log("WARNING: Wrong integer value of the \"level\" tag for prototype with VNUM %d. Leaving default value %d.\n",
					m_vnum, m_level); });
		}

		const auto weight = node->child("weight");
		if (weight)
		{
			CLoadHelper::load_integer(weight.child_value(),
				[&](const auto value) { set_weight(std::max(value, 1)); },
				[&]() { log("WARNING: Wrong integer value of the \"weight\" tag for prototype with VNUM %d. Leaving default value %d.\n",
					m_vnum, this->get_weight()); });
		}

		const auto timer = node->child("timer");
		{
			const std::string timer_value = timer.child_value();
			if ("unlimited" == timer_value)
			{
				set_timer(OBJ_DATA::UNLIMITED_TIMER);
			}
			else
			{
				CLoadHelper::load_integer(weight.child_value(),
					[&](const auto value) { set_timer(std::max(value, 0)); },
					[&]() { log("WARNING: Wrong integer value of the \"timer\" tag for prototype with VNUM %d. Leaving default value %d.\n",
						m_vnum, this->get_timer()); });
			}
		}

		const auto item_parameters = node->child("item_parameters");
		if (item_parameters)
		{
			const bool load_result = load_item_parameters(&item_parameters);
			if (!load_result)
			{
				return false;
			}
		}

		load_result = CLoadHelper::load_flag<decltype(m_material)>(*node, "material",
			[&](const auto material) { m_material = material; },
			[&](const auto name) { log("WARNING: Failed to set material '%s' for prototype with VNUM %d. Prototype will be skipped.\n", name, m_vnum); },
			[&]() { log("WARNING: \"material\" tag for prototype with VNUM %d not found. Setting to default value: %s.\n", m_vnum, NAME_BY_ITEM(m_material).c_str()); });
		if (CLoadHelper::ELFR_FAIL == load_result)
		{
			return false;
		}

		load_result = CLoadHelper::load_flag<decltype(m_spell)>(*node, "spell",
			[&](const auto spell) { m_spell = spell; },
			[&](const auto value) { log("WARNING: Failed to set spell '%s' for prototype with VNUM %d. Spell will not be set.\n", value, m_vnum); },
			[&]() {});

		// loading of prototype extraflags
		CLoadHelper::load_flags<EExtraFlag>(m_extraflags, *node, "extraflags", "extraflag",
			[&](const auto value) { log("Setting extra flag '%s' for prototype with VNUM %d.\n", NAME_BY_ITEM(value).c_str(), m_vnum); },
			[&](const auto flag) { log("WARNING: Skipping extra flag '%s' of prototype with VNUM %d, because this value is not valid.\n", flag, m_vnum); });

        // loading of prototype affectflags
		CLoadHelper::load_flags<EAffectFlag>(m_affect_flags, *node, "affects", "affect",
			[&](const auto value) { log("Setting affectflag '%s' for prototype with VNUM %d.\n", NAME_BY_ITEM(value).c_str(), m_vnum); },
			[&](const auto flag) { log("WARNING: Skipping affectflag '%s' of prototype with VNUM %d, because this value is not valid.\n", flag, m_vnum); });
        
        // loading of prototype antiflags
		CLoadHelper::load_flags<EAntiFlag>(m_anti_flags, *node, "antiflags", "antiflag",
			[&](const auto value) { log("Setting antiflag '%s' for prototype with VNUM %d.\n", NAME_BY_ITEM(value).c_str(), m_vnum); },
			[&](const auto flag) { log("WARNING: Skipping antiflag '%s' of prototype with VNUM %d, because this value is not valid.\n", flag, m_vnum); });

		// loading of prototype noflags
		CLoadHelper::load_flags<ENoFlag>(m_no_flags, *node, "noflags", "noflag",
			[&](const auto value) { log("Setting noflag '%s' for prototype with VNUM %d.\n", NAME_BY_ITEM(value).c_str(), m_vnum); },
			[&](const auto flag) { log("WARNING: Skipping noflag '%s' of prototype with VNUM %d, because this value is not valid.\n", flag, m_vnum); });

		// loading of prototype wearflags
		CLoadHelper::load_flags<EWearFlag>(m_wear_flags, *node, "wearflags", "wearflag",
			[&](const auto value) { log("Setting wearflag '%s' for prototype with VNUM %d.\n", NAME_BY_ITEM(value).c_str(), m_vnum); },
			[&](const auto flag) { log("WARNING: Skipping wearflag '%s' of prototype with VNUM %d, because this value is not valid.\n", flag, m_vnum); });

		// loading of prototype skills
		load_skills(node);

		prefix.change_prefix(END_PREFIX);
		log("End of loading prototype with VNUM %d.\n", m_vnum);

		return true;
	}

	bool CPrototype::load_item_parameters(const pugi::xml_node* node)
	{
		switch (get_type())
		{
		case obj_flag_data::ITEM_INGREDIENT:
			for (const auto flags : node->children("parameter"))
			{
				const char* flag = flags.child_value();
				try
				{
					const auto value = ITEM_BY_NAME<EIngredientFlag>(flag);
					m_item_params |= to_underlying(value);
					log("Setting ingredient flag '%s' for prototype with VNUM %d.\n",
						NAME_BY_ITEM(value).c_str(), m_vnum);
				}
				catch (const std::out_of_range&)
				{
					log("WARNING: Skipping ingredient flag '%s' of prototype with VNUM %d, because this value is not valid.\n",
						flag, m_vnum);
				}
			}
			break;

		case obj_flag_data::ITEM_WEAPON:
		{
			const char* skill_value = node->child_value("parameter");
			try
			{
				m_item_params = to_underlying(ITEM_BY_NAME<ESkill>(skill_value));
			}
			catch (const std::out_of_range&)
			{
				log("WARNING: Failed to set skill value '%s' for prototype with VNUM %d. Prototype will be skipped.\n",
					skill_value, m_vnum);
				return false;
			}
			break;
		}

		default:
			// For other item types "skills" tag should be ignored.
			break;
		}

		return true;
	}

	void CPrototype::load_skills(const pugi::xml_node* node)
	{
		const auto skills = node->child("skills");
		if (skills)
		{
			int counter = 0;
			for (const auto& skill : skills.children("skill"))
			{
				++counter;
				std::stringstream determined_id;
				determined_id << '#' << counter << '-' << suffix(counter);		// by default distinguish skills by number

				bool fail = false;
				const auto id_node = skill.child("id");

				// check ID
				ESkill skill_id = SKILL_INVALID;
				do
				{
					if (!id_node)
					{
						log("WARNING: Skill %s for VNUM %d does not have \"id\" tag.\n",
							determined_id.str().c_str(), m_vnum);
						fail = true;
						break;
					}

					const auto skill_name = id_node.child_value();
					try
					{
						skill_id = ITEM_BY_NAME<ESkill>(skill_name);

						// store determined ID
						determined_id.str(std::string());
						determined_id << '"' << skill_name << '"';
					}
					catch (...)
					{
						log("WARNING: \"id\" value of skill \"%s\" for prototype with VNUM %d is not valid.\n",
							skill_name, m_vnum);
						fail = true;
					}
				} while (false);

				// check value
				int skill_value = -1;
				do
				{
					const auto value_node = skill.child("value");
					if (!value_node)
					{
						log("WARNING: Skill %s for prototype with VNUM %d does not have \"value\" tag.\n",
							determined_id.str().c_str(), m_vnum);
						fail = true;
						break;
					}

					CLoadHelper::load_integer(value_node.child_value(), skill_value,
						[&]() {
						log("WARNING: Skill %s for prototype with VNUM %d has wrong integer value of \"value\" tag.\n",
							determined_id.str().c_str(), m_vnum);
						fail = true;
					});
				} while (false);

				if (0 >= skill_value)
				{
					log("WARNING: Skill %s for prototype with VNUM %d has wrong value %d of the \"value\" tag.\n",
						determined_id.str().c_str(), m_vnum, skill_value);
					fail = true;
				}

				if (fail)
				{
					log("WARNING: Skipping skill %s for prototype with VNUM %d (see errors above).\n",
						determined_id.str().c_str(), m_vnum);
				}
				else
				{
					log("Adding the (skill, value) pair (%s, %d) to prototype with VNUM %d.\n",
						determined_id.str().c_str(), skill_value, m_vnum);
					m_skills.insert(skills_t::value_type(skill_id, skill_value));
				}
			}
		}
	}

	bool CMaterialClass::load(const pugi::xml_node* node)
	{
		log("Loading material class with ID '%s'...\n", m_id.c_str());
		CLogger::CPrefix prefix(log, BODY_PREFIX);

		const auto desc_node = node->child("description");
		if (!desc_node)
		{
			log("ERROR: material class with ID '%s' does not contain required \"description\" tag.\n",
				m_id.c_str());
			return false;
		}

		const auto short_desc = desc_node.child("short");
		if (!short_desc)
		{
			log("ERROR: material class with ID '%s' does not contain required \"description/short\" tag.\n",
					m_id.c_str());
			return false;
		}
		m_short_desc = short_desc.value();

		const auto long_desc = desc_node.child("long");
		if (!long_desc)
		{
			log("ERROR: material class with ID '%s' does not contain required \"description/long\" tag.\n",
					m_id.c_str());
			return false;
		}
		m_long_desc = long_desc.value();

		const auto item = node->child("item");
		if (!item)
		{
			log("ERROR: material class with ID '%s' does not contain required \"item\" tag.\n", m_id.c_str());
			return false;
		}
		if (!m_item_cases.load(&item))
		{
			log("ERROR: could not load item cases for material class '%s'.\n", m_id.c_str());
			return false;
		}

		const auto adjectives = node->child("adjectives");
		if (!adjectives)
		{
			log("ERROR: material class with ID '%s' does not contain required \"adjectives\" tag.\n", m_id.c_str());
			return false;
		}

		const auto male = adjectives.child("male");
		if (!male)
		{
			log("ERROR: material class with ID '%s' does not contain required \"adjectives/male\" tag.\n", m_id.c_str());
			return false;
		}
		if (!m_male_adjectives.load(&male))
		{
			log("ERROR: could not load male adjective cases for material class '%s'.\n", m_id.c_str());
			return false;
		}

		const auto female = adjectives.child("female");
		if (!female)
		{
			log("ERROR: material class with ID '%s' does not contain required \"adjectives/female\" tag.\n", m_id.c_str());
			return false;
		}
		if (!m_female_adjectives.load(&female))
		{
			log("ERROR: could not load female adjective cases for material class '%s'.\n", m_id.c_str());
			return false;
		}

		const auto neuter = adjectives.child("neuter");
		if (!neuter)
		{
			log("ERROR: material class with ID '%s' does not contain required \"adjectives/neuter\" tag.\n", m_id.c_str());
			return false;
		}
		if (!m_neuter_adjectives.load(&neuter))
		{
			log("ERROR: could not load neuter adjective cases for material class '%s'.\n", m_id.c_str());
			return false;
		}

		// load extra flags
		CLoadHelper::load_flags<EExtraFlag>(m_extraflags, *node, "extraflags", "extraflag",
			[&](const auto value) { log("Setting extra flag '%s' for class ID %s.\n", NAME_BY_ITEM(value).c_str(), m_id.c_str()); },
			[&](const auto flag) { log("WARNING: Skipping extra flag '%s' of class with ID %s, because this value is not valid.\n", flag, m_id.c_str()); });

		// load affects
		CLoadHelper::load_flags<EAffectFlag>(m_affect_flags, *node, "affects", "affect",
			[&](const auto value) { log("Setting affect flag '%s' for class ID %s.\n", NAME_BY_ITEM(value).c_str(), m_id.c_str()); },
			[&](const auto flag) { log("WARNING: Skipping affect flag '%s' of class with ID %s, because this value is not valid.\n", flag, m_id.c_str()); });

		prefix.change_prefix(END_PREFIX);
		log("End of loading material class with ID '%s'.\n", m_id.c_str());

		return true;
	}

	bool CMaterial::load(const pugi::xml_node* node)
	{
		log("Loading material with ID %s...\n", m_id.c_str());
		CLogger::CPrefix prefix(log, BODY_PREFIX);

		// load material name
		const auto node_name = node->child("name");
		if (!node_name)
		{
			log("ERROR: could not find required node 'name' for material with ID '%s'.\n", m_id.c_str());
			return false;
		}
		const std::string name = node_name.value();

		// load meterial classes
		for (const auto node_class: node->children("class"))
		{
			if (node_class.attribute("id").empty())
			{
				log("WARNING: class tag of material with ID '%s' does not contain ID of class. Class will be skipped.\n", m_id.c_str());
				continue;
			}
			const std::string class_id = node_class.attribute("id").value();
			CMaterialClass mc(class_id);
			mc.load(&node_class);
		}

		prefix.change_prefix(END_PREFIX);
		log("End of loading material with ID '%s'.\n", m_id.c_str());

		return true;
	}

	bool CRecipe::load(const pugi::xml_node* /*node*/)
	{
		log("Loading recipe with ID %s...\n", m_id.c_str());
		CLogger::CPrefix prefix(log, BODY_PREFIX);

		prefix.change_prefix(END_PREFIX);
		log("End of loading recipe with ID %s\n", m_id.c_str());

		return true;
	}

	bool CSkillBase::load(const pugi::xml_node* /*node*/)
	{
		log("Loading skill with ID %s...\n", m_id.c_str());
		CLogger::CPrefix prefix(log, BODY_PREFIX);

		prefix.change_prefix(END_PREFIX);
		log("End of loading skill with ID %s\n", m_id.c_str());

		return true;
	}

	bool CCraft::load(const pugi::xml_node* /*node*/)
	{
		log("Loading craft with ID %s...\n", m_id.c_str());
		CLogger::CPrefix prefix(log, BODY_PREFIX);

		prefix.change_prefix(END_PREFIX);
		log("End of loading craft with ID %s\n", m_id.c_str());

		return true;
	}

	bool CCraftModel::load()
	{
		log("Loading craft model from file '%s'...\n",
			FILE_NAME.c_str());
		CLogger::CPrefix prefix(log, BODY_PREFIX);

		pugi::xml_document doc;
		const auto result = doc.load_file(FILE_NAME.c_str());

		if (!result)
		{
			log("Craft load error: '%s' at offset %zu\n",
				result.description(),
				result.offset);
			return false;
		}
		
		const auto model = doc.child("craftmodel");
		if (!model)
		{
			log("Craft model is not defined in XML file %s\n", FILE_NAME.c_str());
			return false;
		}
		// Load model properties.
		// TODO: load it.

		// Load materials.
		const auto materials = model.child("materials");
		if (materials)
		{
			size_t number = 0;
			for (const auto material: materials.children("material"))
			{
				load_material(&material, ++number);
			}
		}

		// Load recipes.
		// TODO: load it.

		// Load skills.
		// TODO: load it.

		// Load crafts.
		// TODO: load it.

		// Load prototypes
		const auto prototypes = model.child("prototypes");
		if (prototypes)
		{
			size_t number = 0;
			for (const auto prototype : prototypes.children("prototype"))
			{
				load_prototype(&prototype, ++number);
			}
		}

		prefix.change_prefix(END_PREFIX);
		log("End of loading craft model.\n");
		// TODO: print statistics of the model (i. e. count of materials, recipes, crafts, missed entries and so on).

		return true;
	}

	CLogger log;

	void CCraftModel::create_item() const
	{

	}

	bool CCraftModel::load_prototype(const pugi::xml_node* prototype, const size_t number)
	{
		if (prototype->attribute("vnum").empty())
		{
			log("%zd-%s prototype tag does not have VNUM attribute. Will be skipped.\n",
				number, suffix(number));
			return false;
		}

		vnum_t vnum = prototype->attribute("vnum").as_int(0);
		if (0 == vnum)
		{
			log("Failed to get VNUM of the %zd-%s prototype. This prototype entry will be skipped.\n",
				number, suffix(number));
		}

		CPrototype p(vnum);
		if (prototype->attribute("filename").empty())
		{
			if (!p.load(prototype))
			{
				log("WARNING: Skipping %zd-%s prototype with VNUM %d.\n",
					number, suffix(number), vnum);
				return false;
			}
		}
		else
		{
			using boost::filesystem::path;
			const std::string filename = (path(FILE_NAME).parent_path() / prototype->attribute("filename").value()).string();
			pugi::xml_document pdoc;
			const auto presult = pdoc.load_file(filename.c_str());
			if (!presult)
			{
				log("WARNING: could not load external file '%s' with %zd-%s prototype (ID: %d): '%s' "
					"at offset %zu. Prototype will be skipped.\n",
					filename.c_str(),
					number,
					suffix(number),
					vnum,
					presult.description(),
					presult.offset);
				return false;
			}
			const auto proot = pdoc.child("prototype");
			if (!proot)
			{
				log("WARNING: could not find root \"prototype\" tag for prototype with VNUM "
					"%d in the external file '%s'. Prototype will be skipped.\n",
					vnum,
					filename.c_str());
				return false;
			}
			log("Using external file '%s' for %zd-%s prototype with VNUM %d.\n",
				filename.c_str(),
				number,
				suffix(number),
				vnum);
			if (!p.load(&proot))
			{
				log("WARNING: Skipping %zd-%s prototype with VNUM %d.\n",
					number, suffix(number), vnum);
				return false;
			}
		}

		return true;
	}

	bool CCraftModel::load_material(const pugi::xml_node* material, const size_t number)
	{
		if (material->attribute("id").empty())
		{
			log("%zd-%s material tag does not have ID attribute. Will be skipped.\n",
				number, suffix(number));
			return false;
		}
		id_t id = material->attribute("id").as_string();
		CMaterial m(id);
		if (material->attribute("filename").empty())
		{
			if (!m.load(material))
			{
				log("WARNING: Skipping material with ID '%s'.\n", id.c_str());
				return false;
			}
		}
		else
		{
			using boost::filesystem::path;
			const std::string filename = (path(FILE_NAME).parent_path() / material->attribute("filename").value()).string();
			pugi::xml_document mdoc;
			const auto mresult = mdoc.load_file(filename.c_str());
			if (!mresult)
			{
				log("WARNING: could not load external file '%s' with material '%s': '%s' "
					"at offset %zu. Material will be skipped.\n",
					filename.c_str(),
					id.c_str(),
					mresult.description(),
					mresult.offset);
				return false;
			}
			const auto mroot = mdoc.child("material");
			if (!mroot)
			{
				log("WARNING: could not find root \"material\" tag for material with ID "
					"'%s' in the external file '%s'. Material will be skipped.\n",
					id.c_str(),
					filename.c_str());
				return false;
			}
			log("Using external file '%s' for material with ID '%s'.\n",
				filename.c_str(),
				id.c_str());
			if (!m.load(&mroot))
			{
				log("WARNING: Skipping material with ID '%s'.\n",
					id.c_str());
				return false;
			}
		}

		return true;
	}
}

/* vim: set ts=4 sw=4 tw=0 noet syntax=cpp :*/