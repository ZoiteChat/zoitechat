/* Spell-entry dictionary reload regressions. GPL-2.0-or-later. */
#include "../../common/zoitechat.h"

/* Exercise the actual widget with a deterministic dictionary provider. This
 * must also run on builders without Enchant or installed dictionaries. */
#include "../sexy-spell-entry.c"

struct zoitechatprefs prefs;
struct EnchantBroker { int unused; };
struct EnchantDict { char *language; };
static gboolean gtk_available, dictionaries_available;
static int active_dicts, checked_words;

gboolean theme_get_color (ThemeSemanticToken token, GdkRGBA *color)
{
	return gdk_rgba_parse (color, token == THEME_TOKEN_TEXT_BACKGROUND ? "#ffffff" : "#000000");
}
gboolean theme_get_mirc_color (unsigned int index, GdkRGBA *color)
{
	return gdk_rgba_parse (color, "#000000");
}
void theme_get_widget_style_values_for_widget (GtkWidget *widget, ThemeWidgetStyleValues *values)
{
	memset (values, 0, sizeof *values);
	gdk_rgba_parse (&values->background, "#ffffff");
}
void theme_palette_color_get_rgb16 (const GdkRGBA *color, guint16 *red, guint16 *green, guint16 *blue)
{
	*red = color->red * 65535;
	*green = color->green * 65535;
	*blue = color->blue * 65535;
}
const char *gtkutil_icon_name_from_stock (const char *stock) { return stock; }
GtkWidget *gtkutil_image_new_from_stock (const char *stock, GtkIconSize size)
{
	return gtk_image_new_from_icon_name (stock, size);
}

#ifdef HAVE_ISO_CODES
void codetable_init (void) { }
void codetable_free (void) { }
void codetable_lookup (const gchar *code, const gchar **language, const gchar **country)
{
	*language = code;
	*country = "";
}
#endif

static struct EnchantBroker *test_broker_new (void) { return g_new0 (struct EnchantBroker, 1); }
static void test_broker_free (struct EnchantBroker *broker) { g_free (broker); }
static void test_list_dicts (struct EnchantBroker *broker, EnchantDictDescribeFn describe, void *data)
{
	if (dictionaries_available) describe ("en_US", "test", "test", "", data);
}
static struct EnchantDict *test_request_dict (struct EnchantBroker *broker, const char *const language)
{
	if (!dictionaries_available || (strcmp (language, "en_US") && strcmp (language, "en"))) return NULL;
	struct EnchantDict *dict = g_new0 (struct EnchantDict, 1);
	dict->language = g_strdup (language);
	active_dicts++;
	return dict;
}
static void test_free_dict (struct EnchantBroker *broker, struct EnchantDict *dict)
{
	g_free (dict->language);
	g_free (dict);
	active_dicts--;
}
static void test_describe_dict (struct EnchantDict *dict, EnchantDictDescribeFn describe, void *data)
{
	describe (dict->language, "test", "test", "", data);
}
static void test_add_word (struct EnchantDict *dict, const char *const word, ssize_t length) { }
static int test_check_word (struct EnchantDict *dict, const char *const word, ssize_t length)
{
	checked_words++;
	return 0;
}

static SexySpellEntry *
new_input (gboolean available)
{
	dictionaries_available = available;
	memset (&prefs, 0, sizeof prefs);
	g_strlcpy (prefs.hex_text_spell_langs, "missing_XX", sizeof prefs.hex_text_spell_langs);
	SexySpellEntry *entry = SEXY_SPELL_ENTRY (sexy_spell_entry_new ());
	g_object_ref_sink (entry);
	return entry;
}

static void
destroy_input (SexySpellEntry *entry)
{
	gtk_widget_destroy (GTK_WIDGET (entry));
	g_object_unref (entry);
	g_assert_cmpint (active_dicts, ==, 0);
}

static void
test_first_dictionary_on_empty_input (void)
{
	if (!gtk_available) { g_test_skip ("GTK display unavailable"); return; }
	SexySpellEntry *entry = new_input (FALSE);
	g_assert_null (entry->priv->dict_list);
	/* The real Save path: an untouched input, no old dictionary, then a newly
	 * selected dictionary. Before the fix, the final recheck dereferenced NULL. */
	dictionaries_available = TRUE;
	g_strlcpy (prefs.hex_text_spell_langs, "en_US", sizeof prefs.hex_text_spell_langs);
	for (int i = 0; i < 3; i++)
	{
		sexy_spell_entry_deactivate_language (entry, NULL);
		sexy_spell_entry_activate_default_languages (entry);
		g_assert_true (sexy_spell_entry_language_is_active (entry, "en_US"));
		g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (entry)), ==, "");
	}
	destroy_input (entry);
}

static void
test_reload_preserves_draft (void)
{
	if (!gtk_available) { g_test_skip ("GTK display unavailable"); return; }
	SexySpellEntry *entry = new_input (FALSE);
	const char *draft = "Hello café";
	gtk_entry_set_text (GTK_ENTRY (entry), draft);
	dictionaries_available = TRUE;
	g_strlcpy (prefs.hex_text_spell_langs, "en_US", sizeof prefs.hex_text_spell_langs);
	checked_words = 0;
	sexy_spell_entry_deactivate_language (entry, NULL);
	sexy_spell_entry_activate_default_languages (entry);
	g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (entry)), ==, draft);
	g_assert_cmpint (checked_words, >, 0);
	/* Save-failure rollback and retry also reload the input dictionaries. */
	dictionaries_available = FALSE;
	sexy_spell_entry_deactivate_language (entry, NULL);
	sexy_spell_entry_activate_default_languages (entry);
	dictionaries_available = TRUE;
	sexy_spell_entry_activate_default_languages (entry);
	g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (entry)), ==, draft);
	destroy_input (entry);
}

static void
test_empty_input_spelling_toggles (void)
{
	if (!gtk_available) { g_test_skip ("GTK display unavailable"); return; }
	SexySpellEntry *entry = new_input (TRUE);
	sexy_spell_entry_set_parse_attributes (entry, FALSE);
	sexy_spell_entry_set_checked (entry, FALSE);
	sexy_spell_entry_set_checked (entry, TRUE);
	sexy_spell_entry_set_parse_attributes (entry, TRUE);
	sexy_spell_entry_deactivate_language (entry, NULL);
	sexy_spell_entry_activate_default_languages (entry);
	g_assert_true (sexy_spell_entry_language_is_active (entry, "en"));
	destroy_input (entry);
}

int main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	gtk_available = gtk_init_check (&argc, &argv);
	if (gtk_available)
	{
		SexySpellEntryClass *klass = g_type_class_ref (SEXY_TYPE_SPELL_ENTRY);
		have_enchant = TRUE;
		klass->word_check = default_word_check;
		enchant_broker_init = test_broker_new;
		enchant_broker_free = test_broker_free;
		enchant_broker_list_dicts = test_list_dicts;
		enchant_broker_request_dict = test_request_dict;
		enchant_broker_free_dict = test_free_dict;
		enchant_dict_describe = test_describe_dict;
		enchant_dict_add_to_session = test_add_word;
		enchant_dict_check = test_check_word;
	}
	g_test_add_func ("/spell-entry/first-dictionary-empty-input", test_first_dictionary_on_empty_input);
	g_test_add_func ("/spell-entry/reload-preserves-draft", test_reload_preserves_draft);
	g_test_add_func ("/spell-entry/empty-input-spelling-toggles", test_empty_input_spelling_toggles);
	return g_test_run ();
}
