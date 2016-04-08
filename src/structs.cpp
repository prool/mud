#include "structs.h"

#include "utils.h"

void asciiflag_conv(const char *flag, void *to)
{
	int *flags = (int *)to;
	unsigned int is_number = 1, block = 0, i;
	const char *p;

	for (p = flag; *p; p += i + 1)
	{
		i = 0;
		if (islower(*p))
		{
			if (*(p + 1) >= '0' && *(p + 1) <= '9')
			{
				block = (int)* (p + 1) - '0';
				i = 1;
			}
			else
				block = 0;
			*(flags + block) |= (0x3FFFFFFF & (1 << (*p - 'a')));
		}
		else if (isupper(*p))
		{
			if (*(p + 1) >= '0' && *(p + 1) <= '9')
			{
				block = (int)* (p + 1) - '0';
				i = 1;
			}
			else
				block = 0;
			*(flags + block) |= (0x3FFFFFFF & (1 << (26 + (*p - 'A'))));
		}
		if (!isdigit(*p))
			is_number = 0;
	}

	if (is_number)
	{
		is_number = atol(flag);
		block = is_number < INT_ONE ? 0 : is_number < INT_TWO ? 1 : is_number < INT_THREE ? 2 : 3;
		*(flags + block) = is_number & 0x3FFFFFFF;
	}
}

void FLAG_DATA::from_string(const char *flag)
{
	uint32_t is_number = 1;
	int i;

	for (const char* p = flag; *p; p += i + 1)
	{
		i = 0;
		if (islower(*p))
		{
			size_t block = 0;
			if (*(p + 1) >= '0' && *(p + 1) <= '9')
			{
				block = (int)* (p + 1) - '0';
				i = 1;
			}
			m_flags[block] |= (0x3FFFFFFF & (1 << (*p - 'a')));
		}
		else if (isupper(*p))
		{
			size_t block = 0;
			if (*(p + 1) >= '0' && *(p + 1) <= '9')
			{
				block = (int)* (p + 1) - '0';
				i = 1;
			}
			m_flags[block] |= (0x3FFFFFFF & (1 << (26 + (*p - 'A'))));
		}

		if (!isdigit(*p))
		{
			is_number = 0;
		}
	}

	if (is_number)
	{
		is_number = atol(flag);
		const size_t block = is_number >> 30;
		m_flags[block] = is_number & 0x3FFFFFFF;
	}
}

void FLAG_DATA::tascii(int num_planes, char* ascii) const
{
	bool found = false;

	for (int i = 0; i < num_planes; i++)
	{
		for (int c = 0; c < 30; c++)
		{
			if (m_flags[i] & (1 << c))
			{
				found = true;
				sprintf(ascii + strlen(ascii), "%c%d", c < 26 ? c + 'a' : c - 26 + 'A', i);
			}
		}
	}

	strcat(ascii, found ? " " : "0 ");
}

void tascii(const uint32_t* pointer, int num_planes, char* ascii)
{
	bool found = false;

	for (int i = 0; i < num_planes; i++)
	{
		for (int c = 0; c < 30; c++)
		{
			if (pointer[i] & (1 << c))
			{
				found = true;
				sprintf(ascii + strlen(ascii), "%c%d", c < 26 ? c + 'a' : c - 26 + 'A', i);
			}
		}
	}

	strcat(ascii, found ? " " : "0 ");
}

bool sprintbitwd(bitvector_t bitvector, const char *names[], char *result, const char *div, const int print_flag)
{
	long nr = 0;
	int fail = 0;
	int plane = 0;
	char c = 'a';

	*result = '\0';

	fail = bitvector >> 30;
	bitvector &= 0x3FFFFFFF;
	while (fail)
	{
		if (*names[nr] == '\n')
		{
			fail--;
			plane++;
		}
		nr++;
	}

	bool first = true;
	for (; bitvector; bitvector >>= 1)
	{
		if (IS_SET(bitvector, 1))
		{
			if (!first)
			{
				strcat(result, div);
			}
			first = false;

			if (*names[nr] != '\n')
			{
				if (print_flag == 1)
				{
					sprintf(result + strlen(result), "%c%d:", c, plane);
				}
				if ((print_flag == 2) && (!strcmp(names[nr], "UNUSED")))
				{
					sprintf(result + strlen(result), "%ld:", nr + 1);
				}
				strcat(result, names[nr]);
			}
			else
			{
				if (print_flag == 2)
				{
					sprintf(result + strlen(result), "%ld:", nr + 1);
				}
				else if (print_flag == 1)
				{
					sprintf(result + strlen(result), "%c%d:", c, plane);
				}
				strcat(result, "UNDEF");
			}
		}
		if (print_flag == 1)
		{
			c++;
			if (c > 'z')
			{
				c = 'A';
			}
		}
		if (*names[nr] != '\n')
		{
			nr++;
		}
	}

	return '\0' != *result;
}

bool FLAG_DATA::sprintbits(const char *names[], char *result, const char *div, const int print_flag) const
{
	bool have_flags = false;
	char buffer[MAX_STRING_LENGTH];
	*result = '\0';

	for (int i = 0; i < 4; i++)
	{
		if (sprintbitwd(m_flags[i] | (i << 30), names, buffer, div, print_flag))
		{
			if ('\0' != *result)	// We don't need to calculate length of result. We just want to know if it is not empty.
			{
				strcat(result, div);
			}
			strcat(result, buffer);
			have_flags = true;
		}
	}

	return have_flags;
}

const religion_names_t religion_name =
{
	religion_genders_t{ "�������", "�������", "��������", "��������" },
	religion_genders_t{ "����������", "����������", "����������", "���������" }
};

typedef std::map<ESex, std::string> ESex_name_by_value_t;
typedef std::map<const std::string, ESex> ESex_value_by_name_t;
ESex_name_by_value_t ESex_name_by_value;
ESex_value_by_name_t ESex_value_by_name;

void init_ESex_ITEM_NAMES()
{
	ESex_name_by_value.clear();
	ESex_value_by_name.clear();

	ESex_name_by_value[ESex::SEX_NEUTRAL] = "NEUTRAL";
	ESex_name_by_value[ESex::SEX_MALE] = "MALE";
	ESex_name_by_value[ESex::SEX_FEMALE] = "FEMALE";
	ESex_name_by_value[ESex::SEX_POLY] = "POLY";

	for (const auto& i : ESex_name_by_value)
	{
		ESex_value_by_name[i.second] = i.first;
	}
}

template <>
ESex ITEM_BY_NAME(const std::string& name)
{
	if (ESex_name_by_value.empty())
	{
		init_ESex_ITEM_NAMES();
	}
	return ESex_value_by_name.at(name);
}

template <>
const std::string& NAME_BY_ITEM(const ESex item)
{
	if (ESex_name_by_value.empty())
	{
		init_ESex_ITEM_NAMES();
	}
	return ESex_name_by_value.at(item);
}

typedef std::map<EWearFlag, std::string> EWearFlag_name_by_value_t;
typedef std::map<const std::string, EWearFlag> EWearFlag_value_by_name_t;
EWearFlag_name_by_value_t EWearFlag_name_by_value;
EWearFlag_value_by_name_t EWearFlag_value_by_name;

void init_EWearFlag_ITEM_NAMES()
{
	EWearFlag_name_by_value.clear();
	EWearFlag_value_by_name.clear();

	EWearFlag_name_by_value[EWearFlag::ITEM_WEAR_TAKE] = "ITEM_WEAR_TAKE";
	EWearFlag_name_by_value[EWearFlag::ITEM_WEAR_FINGER] = "ITEM_WEAR_FINGER";
	EWearFlag_name_by_value[EWearFlag::ITEM_WEAR_NECK] = "ITEM_WEAR_NECK";
	EWearFlag_name_by_value[EWearFlag::ITEM_WEAR_BODY] = "ITEM_WEAR_BODY";
	EWearFlag_name_by_value[EWearFlag::ITEM_WEAR_HEAD] = "ITEM_WEAR_HEAD";
	EWearFlag_name_by_value[EWearFlag::ITEM_WEAR_LEGS] = "ITEM_WEAR_LEGS";
	EWearFlag_name_by_value[EWearFlag::ITEM_WEAR_FEET] = "ITEM_WEAR_FEET";
	EWearFlag_name_by_value[EWearFlag::ITEM_WEAR_HANDS] = "ITEM_WEAR_HANDS";
	EWearFlag_name_by_value[EWearFlag::ITEM_WEAR_ARMS] = "ITEM_WEAR_ARMS";
	EWearFlag_name_by_value[EWearFlag::ITEM_WEAR_SHIELD] = "ITEM_WEAR_SHIELD";
	EWearFlag_name_by_value[EWearFlag::ITEM_WEAR_ABOUT] = "ITEM_WEAR_ABOUT";
	EWearFlag_name_by_value[EWearFlag::ITEM_WEAR_WAIST] = "ITEM_WEAR_WAIST";
	EWearFlag_name_by_value[EWearFlag::ITEM_WEAR_WRIST] = "ITEM_WEAR_WRIST";
	EWearFlag_name_by_value[EWearFlag::ITEM_WEAR_WIELD] = "ITEM_WEAR_WIELD";
	EWearFlag_name_by_value[EWearFlag::ITEM_WEAR_HOLD] = "ITEM_WEAR_HOLD";
	EWearFlag_name_by_value[EWearFlag::ITEM_WEAR_BOTHS] = "ITEM_WEAR_BOTHS";

	for (const auto& i : EWearFlag_name_by_value)
	{
		EWearFlag_value_by_name[i.second] = i.first;
	}
}

template <> const std::string& NAME_BY_ITEM<EWearFlag>(const EWearFlag item)
{
	if (EWearFlag_name_by_value.empty())
	{
		init_EWearFlag_ITEM_NAMES();
	}
	return EWearFlag_name_by_value.at(item);
}

template <> EWearFlag ITEM_BY_NAME<EWearFlag>(const std::string& name)
{
	if (EWearFlag_name_by_value.empty())
	{
		init_EWearFlag_ITEM_NAMES();
	}
	return EWearFlag_value_by_name.at(name);
}

typedef std::map<EExtraFlag, std::string> EExtraFlag_name_by_value_t;
typedef std::map<const std::string, EExtraFlag> EExtraFlag_value_by_name_t;
EExtraFlag_name_by_value_t EExtraFlag_name_by_value;
EExtraFlag_value_by_name_t EExtraFlag_value_by_name;

void init_EExtraFlag_ITEM_NAMES()
{
	EExtraFlag_name_by_value.clear();
	EExtraFlag_value_by_name.clear();

	EExtraFlag_name_by_value[EExtraFlag::ITEM_GLOW] = "ITEM_GLOW";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_HUM] = "ITEM_HUM";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_NORENT] = "ITEM_NORENT";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_NODONATE] = "ITEM_NODONATE";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_NOINVIS] = "ITEM_NOINVIS";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_INVISIBLE] = "ITEM_INVISIBLE";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_MAGIC] = "ITEM_MAGIC";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_NODROP] = "ITEM_NODROP";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_BLESS] = "ITEM_BLESS";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_NOSELL] = "ITEM_NOSELL";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_DECAY] = "ITEM_DECAY";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_ZONEDECAY] = "ITEM_ZONEDECAY";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_NODISARM] = "ITEM_NODISARM";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_NODECAY] = "ITEM_NODECAY";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_POISONED] = "ITEM_POISONED";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_SHARPEN] = "ITEM_SHARPEN";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_ARMORED] = "ITEM_ARMORED";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_DAY] = "ITEM_DAY";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_NIGHT] = "ITEM_NIGHT";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_FULLMOON] = "ITEM_FULLMOON";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_WINTER] = "ITEM_WINTER";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_SPRING] = "ITEM_SPRING";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_SUMMER] = "ITEM_SUMMER";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_AUTUMN] = "ITEM_AUTUMN";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_SWIMMING] = "ITEM_SWIMMING";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_FLYING] = "ITEM_FLYING";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_THROWING] = "ITEM_THROWING";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_TICKTIMER] = "ITEM_TICKTIMER";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_FIRE] = "ITEM_FIRE";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_REPOP_DECAY] = "ITEM_REPOP_DECAY";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_NOLOCATE] = "ITEM_NOLOCATE";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_TIMEDLVL] = "ITEM_TIMEDLVL";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_NOALTER] = "ITEM_NOALTER";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_WITH1SLOT] = "ITEM_WITH1SLOT";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_WITH2SLOTS] = "ITEM_WITH2SLOTS";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_WITH3SLOTS] = "ITEM_WITH3SLOTS";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_SETSTUFF] = "ITEM_SETSTUFF";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_NO_FAIL] = "ITEM_NO_FAIL";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_NAMED] = "ITEM_NAMED";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_BLOODY] = "ITEM_BLOODY";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_1INLAID] = "ITEM_1INLAID";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_2INLAID] = "ITEM_2INLAID";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_3INLAID] = "ITEM_3INLAID";
	EExtraFlag_name_by_value[EExtraFlag::ITEM_NOPOUR] = "ITEM_NOPOUR";

	for (const auto& i : EExtraFlag_name_by_value)
	{
		EExtraFlag_value_by_name[i.second] = i.first;
	}
}

template <>
const std::string& NAME_BY_ITEM(const EExtraFlag item)
{
	if (EExtraFlag_name_by_value.empty())
	{
		init_EExtraFlag_ITEM_NAMES();
	}
	return EExtraFlag_name_by_value.at(item);
}

template <>
EExtraFlag ITEM_BY_NAME(const std::string& name)
{
    if (EExtraFlag_name_by_value.empty())
    {
        init_EExtraFlag_ITEM_NAMES();
    }
    return EExtraFlag_value_by_name.at(name);
}

typedef std::map<EAffectFlag, std::string> EAffectFlag_name_by_value_t;
typedef std::map<const std::string, EAffectFlag> EAffectFlag_value_by_name_t;
EAffectFlag_name_by_value_t EAffectFlag_name_by_value;
EAffectFlag_value_by_name_t EAffectFlag_value_by_name;
void init_EAffectFlag_ITEM_NAMES()
{
	EAffectFlag_value_by_name.clear();
	EAffectFlag_name_by_value.clear();

	EAffectFlag_name_by_value[EAffectFlag::AFF_BLIND] = "AFF_BLIND";
	EAffectFlag_name_by_value[EAffectFlag::AFF_INVISIBLE] = "AFF_INVISIBLE";
	EAffectFlag_name_by_value[EAffectFlag::AFF_DETECT_ALIGN] = "AFF_DETECT_ALIGN";
	EAffectFlag_name_by_value[EAffectFlag::AFF_DETECT_INVIS] = "AFF_DETECT_INVIS";
	EAffectFlag_name_by_value[EAffectFlag::AFF_DETECT_MAGIC] = "AFF_DETECT_MAGIC";
	EAffectFlag_name_by_value[EAffectFlag::AFF_SENSE_LIFE] = "AFF_SENSE_LIFE";
	EAffectFlag_name_by_value[EAffectFlag::AFF_WATERWALK] = "AFF_WATERWALK";
	EAffectFlag_name_by_value[EAffectFlag::AFF_SANCTUARY] = "AFF_SANCTUARY";
	EAffectFlag_name_by_value[EAffectFlag::AFF_GROUP] = "AFF_GROUP";
	EAffectFlag_name_by_value[EAffectFlag::AFF_CURSE] = "AFF_CURSE";
	EAffectFlag_name_by_value[EAffectFlag::AFF_INFRAVISION] = "AFF_INFRAVISION";
	EAffectFlag_name_by_value[EAffectFlag::AFF_POISON] = "AFF_POISON";
	EAffectFlag_name_by_value[EAffectFlag::AFF_PROTECT_EVIL] = "AFF_PROTECT_EVIL";
	EAffectFlag_name_by_value[EAffectFlag::AFF_PROTECT_GOOD] = "AFF_PROTECT_GOOD";
	EAffectFlag_name_by_value[EAffectFlag::AFF_SLEEP] = "AFF_SLEEP";
	EAffectFlag_name_by_value[EAffectFlag::AFF_NOTRACK] = "AFF_NOTRACK";
	EAffectFlag_name_by_value[EAffectFlag::AFF_TETHERED] = "AFF_TETHERED";
	EAffectFlag_name_by_value[EAffectFlag::AFF_BLESS] = "AFF_BLESS";
	EAffectFlag_name_by_value[EAffectFlag::AFF_SNEAK] = "AFF_SNEAK";
	EAffectFlag_name_by_value[EAffectFlag::AFF_HIDE] = "AFF_HIDE";
	EAffectFlag_name_by_value[EAffectFlag::AFF_COURAGE] = "AFF_COURAGE";
	EAffectFlag_name_by_value[EAffectFlag::AFF_CHARM] = "AFF_CHARM";
	EAffectFlag_name_by_value[EAffectFlag::AFF_HOLD] = "AFF_HOLD";
	EAffectFlag_name_by_value[EAffectFlag::AFF_FLY] = "AFF_FLY";
	EAffectFlag_name_by_value[EAffectFlag::AFF_SIELENCE] = "AFF_SIELENCE";
	EAffectFlag_name_by_value[EAffectFlag::AFF_AWARNESS] = "AFF_AWARNESS";
	EAffectFlag_name_by_value[EAffectFlag::AFF_BLINK] = "AFF_BLINK";
	EAffectFlag_name_by_value[EAffectFlag::AFF_HORSE] = "AFF_HORSE";
	EAffectFlag_name_by_value[EAffectFlag::AFF_NOFLEE] = "AFF_NOFLEE";
	EAffectFlag_name_by_value[EAffectFlag::AFF_SINGLELIGHT] = "AFF_SINGLELIGHT";
	EAffectFlag_name_by_value[EAffectFlag::AFF_HOLYLIGHT] = "AFF_HOLYLIGHT";
	EAffectFlag_name_by_value[EAffectFlag::AFF_HOLYDARK] = "AFF_HOLYDARK";
	EAffectFlag_name_by_value[EAffectFlag::AFF_DETECT_POISON] = "AFF_DETECT_POISON";
	EAffectFlag_name_by_value[EAffectFlag::AFF_DRUNKED] = "AFF_DRUNKED";
	EAffectFlag_name_by_value[EAffectFlag::AFF_ABSTINENT] = "AFF_ABSTINENT";
	EAffectFlag_name_by_value[EAffectFlag::AFF_STOPRIGHT] = "AFF_STOPRIGHT";
	EAffectFlag_name_by_value[EAffectFlag::AFF_STOPLEFT] = "AFF_STOPLEFT";
	EAffectFlag_name_by_value[EAffectFlag::AFF_STOPFIGHT] = "AFF_STOPFIGHT";
	EAffectFlag_name_by_value[EAffectFlag::AFF_HAEMORRAGIA] = "AFF_HAEMORRAGIA";
	EAffectFlag_name_by_value[EAffectFlag::AFF_CAMOUFLAGE] = "AFF_CAMOUFLAGE";
	EAffectFlag_name_by_value[EAffectFlag::AFF_WATERBREATH] = "AFF_WATERBREATH";
	EAffectFlag_name_by_value[EAffectFlag::AFF_SLOW] = "AFF_SLOW";
	EAffectFlag_name_by_value[EAffectFlag::AFF_HASTE] = "AFF_HASTE";
	EAffectFlag_name_by_value[EAffectFlag::AFF_SHIELD] = "AFF_SHIELD";
	EAffectFlag_name_by_value[EAffectFlag::AFF_AIRSHIELD] = "AFF_AIRSHIELD";
	EAffectFlag_name_by_value[EAffectFlag::AFF_FIRESHIELD] = "AFF_FIRESHIELD";
	EAffectFlag_name_by_value[EAffectFlag::AFF_ICESHIELD] = "AFF_ICESHIELD";
	EAffectFlag_name_by_value[EAffectFlag::AFF_MAGICGLASS] = "AFF_MAGICGLASS";
	EAffectFlag_name_by_value[EAffectFlag::AFF_STAIRS] = "AFF_STAIRS";
	EAffectFlag_name_by_value[EAffectFlag::AFF_STONEHAND] = "AFF_STONEHAND";
	EAffectFlag_name_by_value[EAffectFlag::AFF_PRISMATICAURA] = "AFF_PRISMATICAURA";
	EAffectFlag_name_by_value[EAffectFlag::AFF_HELPER] = "AFF_HELPER";
	EAffectFlag_name_by_value[EAffectFlag::AFF_EVILESS] = "AFF_EVILESS";
	EAffectFlag_name_by_value[EAffectFlag::AFF_AIRAURA] = "AFF_AIRAURA";
	EAffectFlag_name_by_value[EAffectFlag::AFF_FIREAURA] = "AFF_FIREAURA";
	EAffectFlag_name_by_value[EAffectFlag::AFF_ICEAURA] = "AFF_ICEAURA";
	EAffectFlag_name_by_value[EAffectFlag::AFF_DEAFNESS] = "AFF_DEAFNESS";
	EAffectFlag_name_by_value[EAffectFlag::AFF_CRYING] = "AFF_CRYING";
	EAffectFlag_name_by_value[EAffectFlag::AFF_PEACEFUL] = "AFF_PEACEFUL";
	EAffectFlag_name_by_value[EAffectFlag::AFF_MAGICSTOPFIGHT] = "AFF_MAGICSTOPFIGHT";
	EAffectFlag_name_by_value[EAffectFlag::AFF_BERSERK] = "AFF_BERSERK";
	EAffectFlag_name_by_value[EAffectFlag::AFF_LIGHT_WALK] = "AFF_LIGHT_WALK";
	EAffectFlag_name_by_value[EAffectFlag::AFF_BROKEN_CHAINS] = "AFF_BROKEN_CHAINS";
	EAffectFlag_name_by_value[EAffectFlag::AFF_CLOUD_OF_ARROWS] = "AFF_CLOUD_OF_ARROWS";
	EAffectFlag_name_by_value[EAffectFlag::AFF_SHADOW_CLOAK] = "AFF_SHADOW_CLOAK";
	EAffectFlag_name_by_value[EAffectFlag::AFF_GLITTERDUST] = "AFF_GLITTERDUST";
	EAffectFlag_name_by_value[EAffectFlag::AFF_AFFRIGHT] = "AFF_AFFRIGHT";
	EAffectFlag_name_by_value[EAffectFlag::AFF_SCOPOLIA_POISON] = "AFF_SCOPOLIA_POISON";
	EAffectFlag_name_by_value[EAffectFlag::AFF_DATURA_POISON] = "AFF_DATURA_POISON";
	EAffectFlag_name_by_value[EAffectFlag::AFF_SKILLS_REDUCE] = "AFF_SKILLS_REDUCE";
	EAffectFlag_name_by_value[EAffectFlag::AFF_NOT_SWITCH] = "AFF_NOT_SWITCH";
	EAffectFlag_name_by_value[EAffectFlag::AFF_BELENA_POISON] = "AFF_BELENA_POISON";
	EAffectFlag_name_by_value[EAffectFlag::AFF_NOTELEPORT] = "AFF_NOTELEPORT";
	EAffectFlag_name_by_value[EAffectFlag::AFF_LACKY] = "AFF_LACKY";
	EAffectFlag_name_by_value[EAffectFlag::AFF_BANDAGE] = "AFF_BANDAGE";
	EAffectFlag_name_by_value[EAffectFlag::AFF_NO_BANDAGE] = "AFF_NO_BANDAGE";
	EAffectFlag_name_by_value[EAffectFlag::AFF_MORPH] = "AFF_MORPH";
	EAffectFlag_name_by_value[EAffectFlag::AFF_STRANGLED] = "AFF_STRANGLED";
	EAffectFlag_name_by_value[EAffectFlag::AFF_RECALL_SPELLS] = "AFF_RECALL_SPELLS";
	EAffectFlag_name_by_value[EAffectFlag::AFF_NOOB_REGEN] = "AFF_NOOB_REGEN";
	EAffectFlag_name_by_value[EAffectFlag::AFF_VAMPIRE] = "AFF_VAMPIRE";

	for (const auto& i : EAffectFlag_name_by_value)
	{
		EAffectFlag_value_by_name[i.second] = i.first;
	}
}

template <>
const std::string& NAME_BY_ITEM(const EAffectFlag item)
{
	if (EAffectFlag_name_by_value.empty())
	{
		init_EAffectFlag_ITEM_NAMES();
	}
	return EAffectFlag_name_by_value.at(item);
}

template <>
EAffectFlag ITEM_BY_NAME(const std::string& name)
{
	if (EAffectFlag_name_by_value.empty())
	{
		init_EAffectFlag_ITEM_NAMES();
	}
	return EAffectFlag_value_by_name.at(name);
}

typedef std::map<ENoFlag, std::string> ENoFlag_name_by_value_t;
typedef std::map<const std::string, ENoFlag> ENoFlag_value_by_name_t;
ENoFlag_name_by_value_t ENoFlag_name_by_value;
ENoFlag_value_by_name_t ENoFlag_value_by_name;
void init_ENoFlag_ITEM_NAMES()
{
	ENoFlag_value_by_name.clear();
	ENoFlag_name_by_value.clear();

	ENoFlag_name_by_value[ENoFlag::ITEM_NO_MONO] = "ITEM_NO_MONO";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_POLY] = "ITEM_NO_POLY";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_NEUTRAL] = "ITEM_NO_NEUTRAL";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_MAGIC_USER] = "ITEM_NO_MAGIC_USER";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_CLERIC] = "ITEM_NO_CLERIC";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_THIEF] = "ITEM_NO_THIEF";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_WARRIOR] = "ITEM_NO_WARRIOR";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_ASSASINE] = "ITEM_NO_ASSASINE";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_GUARD] = "ITEM_NO_GUARD";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_PALADINE] = "ITEM_NO_PALADINE";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_RANGER] = "ITEM_NO_RANGER";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_SMITH] = "ITEM_NO_SMITH";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_MERCHANT] = "ITEM_NO_MERCHANT";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_DRUID] = "ITEM_NO_DRUID";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_BATTLEMAGE] = "ITEM_NO_BATTLEMAGE";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_CHARMMAGE] = "ITEM_NO_CHARMMAGE";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_DEFENDERMAGE] = "ITEM_NO_DEFENDERMAGE";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_NECROMANCER] = "ITEM_NO_NECROMANCER";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_KILLER] = "ITEM_NO_KILLER";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_COLORED] = "ITEM_NO_COLORED";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_BD] = "ITEM_NO_BD";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_MALE] = "ITEM_NO_MALE";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_FEMALE] = "ITEM_NO_FEMALE";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_CHARMICE] = "ITEM_NO_CHARMICE";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_POLOVCI] = "ITEM_NO_POLOVCI";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_PECHENEGI] = "ITEM_NO_PECHENEGI";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_MONGOLI] = "ITEM_NO_MONGOLI";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_YIGURI] = "ITEM_NO_YIGURI";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_KANGARI] = "ITEM_NO_KANGARI";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_XAZARI] = "ITEM_NO_XAZARI";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_SVEI] = "ITEM_NO_SVEI";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_DATCHANE] = "ITEM_NO_DATCHANE";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_GETTI] = "ITEM_NO_GETTI";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_UTTI] = "ITEM_NO_UTTI";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_XALEIGI] = "ITEM_NO_XALEIGI";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_NORVEZCI] = "ITEM_NO_NORVEZCI";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_RUSICHI] = "ITEM_NO_RUSICHI";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_STEPNYAKI] = "ITEM_NO_STEPNYAKI";
	ENoFlag_name_by_value[ENoFlag::ITEM_NO_VIKINGI] = "ITEM_NO_VIKINGI";

	for (const auto& i : ENoFlag_name_by_value)
	{
		ENoFlag_value_by_name[i.second] = i.first;
	}
}

template <> const std::string& NAME_BY_ITEM<ENoFlag>(const ENoFlag item)
{
	if (ENoFlag_name_by_value.empty())
	{
		init_ENoFlag_ITEM_NAMES();
	}
	return ENoFlag_name_by_value.at(item);
}

template <> ENoFlag ITEM_BY_NAME<ENoFlag>(const std::string& name)
{
	if (ENoFlag_name_by_value.empty())
	{
		init_ENoFlag_ITEM_NAMES();
	}
	return ENoFlag_value_by_name.at(name);
}

typedef std::map<EAntiFlag, std::string> EAntiFlag_name_by_value_t;
typedef std::map<const std::string, EAntiFlag> EAntiFlag_value_by_name_t;
EAntiFlag_name_by_value_t EAntiFlag_name_by_value;
EAntiFlag_value_by_name_t EAntiFlag_value_by_name;
void init_EAntiFlag_ITEM_NAMES()
{
    EAntiFlag_value_by_name.clear();
    EAntiFlag_name_by_value.clear();

    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_MONO] = "ITEM_AN_MONO";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_POLY] = "ITEM_AN_POLY";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_NEUTRAL] = "ITEM_AN_NEUTRAL";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_MAGIC_USER] = "ITEM_AN_MAGIC_USER";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_CLERIC] = "ITEM_AN_CLERIC";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_THIEF] = "ITEM_AN_THIEF";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_WARRIOR] = "ITEM_AN_WARRIOR";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_ASSASINE] = "ITEM_AN_ASSASINE";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_GUARD] = "ITEM_AN_GUARD";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_PALADINE] = "ITEM_AN_PALADINE";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_RANGER] = "ITEM_AN_RANGER";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_SMITH] = "ITEM_AN_SMITH";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_MERCHANT] = "ITEM_AN_MERCHANT";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_DRUID] = "ITEM_AN_DRUID";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_BATTLEMAGE] = "ITEM_AN_BATTLEMAGE";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_CHARMMAGE] = "ITEM_AN_CHARMMAGE";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_DEFENDERMAGE] = "ITEM_AN_DEFENDERMAGE";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_NECROMANCER] = "ITEM_AN_NECROMANCER";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_KILLER] = "ITEM_AN_KILLER";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_COLORED] = "ITEM_AN_COLORED";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_BD] = "ITEM_AN_BD";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_SEVERANE] = "ITEM_AN_SEVERANE";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_POLANE] = "ITEM_AN_POLANE";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_KRIVICHI] = "ITEM_AN_KRIVICHI";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_VATICHI] = "ITEM_AN_VATICHI";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_VELANE] = "ITEM_AN_VELANE";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_DREVLANE] = "ITEM_AN_DREVLANE";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_MALE] = "ITEM_AN_MALE";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_FEMALE] = "ITEM_AN_FEMALE";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_CHARMICE] = "ITEM_AN_CHARMICE";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_POLOVCI] = "ITEM_AN_POLOVCI";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_PECHENEGI] = "ITEM_AN_PECHENEGI";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_MONGOLI] = "ITEM_AN_MONGOLI";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_YIGURI] = "ITEM_AN_YIGURI";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_KANGARI] = "ITEM_AN_KANGARI";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_XAZARI] = "ITEM_AN_XAZARI";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_SVEI] = "ITEM_AN_SVEI";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_DATCHANE] = "ITEM_AN_DATCHANE";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_GETTI] = "ITEM_AN_GETTI";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_UTTI] = "ITEM_AN_UTTI";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_XALEIGI] = "ITEM_AN_XALEIGI";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_NORVEZCI] = "ITEM_AN_NORVEZCI";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_RUSICHI] = "ITEM_AN_RUSICHI";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_STEPNYAKI] = "ITEM_AN_STEPNYAKI";
    EAntiFlag_name_by_value[EAntiFlag::ITEM_AN_VIKINGI] = "ITEM_AN_VIKINGI";

    for (const auto& i : EAntiFlag_name_by_value)
    {
        EAntiFlag_value_by_name[i.second] = i.first;
    }
}

template <>
const std::string& NAME_BY_ITEM<EAntiFlag>(const EAntiFlag item)
{
    if (EAntiFlag_name_by_value.empty())
    {
        init_EAntiFlag_ITEM_NAMES();
    }
    return EAntiFlag_name_by_value.at(item);
}

template <>
EAntiFlag ITEM_BY_NAME(const std::string& name)
{
    if (EAntiFlag_name_by_value.empty())
    {
        init_EAntiFlag_ITEM_NAMES();
    }
    return EAntiFlag_value_by_name.at(name);
}