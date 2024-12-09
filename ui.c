#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define CONFIG_FILE "keyboard_config.txt"
#define KEY_FILE "/tmp/key_pressed"

/* The model columns */
enum {
    COL_ACTIONS = 0,
    COL_DELAY,
    COL_COMMAND,
    NUM_COLS
};

/* Global references */
static GtkWidget *log_label;
static GtkListStore *store;
static GtkCellEditable *current_editor = NULL;
static gint current_editing_column = -1; 

/* Forward declarations */
static void cell_edited_callback(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer user_data);
static void on_editing_started(GtkCellRenderer *renderer, GtkCellEditable *editable, const gchar *path, gpointer user_data);
static void on_editing_canceled(GtkCellEditable *editable, gpointer user_data);
static void on_add_entry_button_clicked(GtkButton *button, gpointer user_data);
static void on_delete_entry_button_clicked(GtkButton *button, gpointer user_data);
static void on_save_button_clicked(GtkButton *button, gpointer user_data);
static void on_file_changed(GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data);
static void parse_key_file_and_update_ui(void);
static gboolean is_actions_text_valid(const gchar *text);

/* Parse config lines */
static gboolean parse_config_line(const char *line, char **actions_out, char **delay_out, char **command_out) {
    const char *eq = strchr(line, '=');
    if (!eq) return FALSE;

    *actions_out = g_strndup(line, eq - line);

    const char *after_eq = eq + 1;
    const char *colon = strchr(after_eq, ':');
    if (!colon) {
        g_free(*actions_out);
        return FALSE;
    }

    char *delay = g_strndup(after_eq, colon - after_eq);
    const char *cmd = colon + 1;
    if (!cmd || *cmd == '\0') {
        g_free(delay);
        g_free(*actions_out);
        *actions_out = NULL;
        return FALSE;
    }

    *delay_out = delay;
    *command_out = g_strdup(cmd);

    return TRUE;
}

/* Load config file */
static void load_config(GtkListStore *store) {
    g_return_if_fail(GTK_IS_LIST_STORE(store));

    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) {
        g_warning("Could not open config file: %s", CONFIG_FILE);
        return;
    }

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        g_strchomp(line);
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }

        char *actions = NULL;
        char *delay = NULL;
        char *command = NULL;
        if (parse_config_line(line, &actions, &delay, &command)) {
            GtkTreeIter iter;
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                               COL_ACTIONS, actions,
                               COL_DELAY, delay,
                               COL_COMMAND, command,
                               -1);
            g_free(actions);
            g_free(delay);
            g_free(command);
        } else {
            g_warning("Failed to parse line: %s", line);
        }
    }
    fclose(f);
}

/* Save config file */
static void save_config(GtkListStore *store) {
    g_return_if_fail(GTK_IS_LIST_STORE(store));

    FILE *f = fopen(CONFIG_FILE, "w");
    if (!f) {
        g_warning("Could not write to config file: %s", CONFIG_FILE);
        return;
    }

    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
    while (valid) {
        gchar *actions = NULL;
        gchar *delay = NULL;
        gchar *command = NULL;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                           COL_ACTIONS, &actions,
                           COL_DELAY, &delay,
                           COL_COMMAND, &command,
                           -1);

        if (actions && delay && command && *actions != '\0' && *delay != '\0' && *command != '\0') {
            fprintf(f, "%s=%s:%s\n", actions, delay, command);
        } else {
            g_warning("Skipping incomplete entry: actions=%s delay=%s command=%s", actions, delay, command);
        }

        g_free(actions);
        g_free(delay);
        g_free(command);

        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
    }

    fclose(f);
    g_message("Configuration saved to %s.", CONFIG_FILE);
}

/* Add entry */
static void on_add_entry_button_clicked(GtkButton *button, gpointer user_data) {
    GtkListStore *store = GTK_LIST_STORE(user_data);

    GtkTreeIter iter;
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COL_ACTIONS, "",
                       COL_DELAY, "",
                       COL_COMMAND, "",
                       -1);
}

/* Delete entry */
static void on_delete_entry_button_clicked(GtkButton *button, gpointer user_data) {
    GtkWidget *treeview = GTK_WIDGET(user_data);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
    } else {
        g_message("No entry selected for deletion.");
    }
}

/* Save button */
static void on_save_button_clicked(GtkButton *button, gpointer user_data) {
    GtkListStore *store = GTK_LIST_STORE(user_data);
    save_config(store);
}

/* Validate actions text */
static gboolean is_actions_text_valid(const gchar *text) {
    for (const gchar *p = text; *p; p++) {
        if (!g_ascii_isdigit(*p) && *p != ',')
            return FALSE; 
    }
    return TRUE;
}

/* Cell edited callback: Filter Actions column input to only digits and commas */
/* Cell edited callback: Filter Actions column input to only digits and commas, no leading commas */
static void cell_edited_callback(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer user_data) {
    GtkListStore *store = GTK_LIST_STORE(user_data);
    GtkTreePath *tree_path = gtk_tree_path_new_from_string(path);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, tree_path)) {
        gint column = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(renderer), "column"));

        if (column == COL_ACTIONS) {
            /* Filter out non-digit and non-comma chars */
            GString *filtered = g_string_new(NULL);
            for (const gchar *p = new_text; *p; p++) {
                if (g_ascii_isdigit(*p) || *p == ',') {
                    g_string_append_c(filtered, *p);
                }
            }

            /* Remove leading commas */
            while (filtered->str[0] == ',') {
                g_string_erase(filtered, 0, 1);
            }

            /* Set the filtered value */
            gtk_list_store_set(store, &iter, column, filtered->str, -1);
            g_string_free(filtered, TRUE);
        } else {
            /* For other columns, just set the value directly */
            gtk_list_store_set(store, &iter, column, new_text, -1);
        }
    }
    gtk_tree_path_free(tree_path);

    /* Editing done, reset the editor pointers */
    current_editor = NULL;
    current_editing_column = -1;
}



/* Called when editing starts */
static void on_editing_started(GtkCellRenderer *renderer, GtkCellEditable *editable, const gchar *path, gpointer user_data) {
    current_editor = editable;
    gint column = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(renderer), "column"));
    current_editing_column = column;

    // Connect "editing-done" and "editing-canceled" signals to reset pointers
    g_signal_connect(editable, "editing-canceled", G_CALLBACK(on_editing_canceled), NULL);
}

/* Called if editing is canceled */
static void on_editing_canceled(GtkCellEditable *editable, gpointer user_data) {
    // Editing canceled, reset the editor pointers
    current_editor = NULL;
    current_editing_column = -1;
}

/* Parse the file and update UI */
static void parse_key_file_and_update_ui(void) {
    gchar *contents = NULL;
    gsize length = 0;
    GError *error = NULL;

    if (!g_file_get_contents(KEY_FILE, &contents, &length, &error)) {
        g_clear_error(&error);
        gtk_label_set_text(GTK_LABEL(log_label), "No key data available.");
        return;
    }

    gtk_label_set_text(GTK_LABEL(log_label), contents);

    // Only modify the Actions cell if currently editing the Actions column
    if (current_editor && current_editing_column == COL_ACTIONS) {
        gchar **lines = g_strsplit(contents, "\n", -1);
        const char *line = NULL;
        for (int i = 0; lines[i] != NULL; i++) {
            if (lines[i][0] != '\0') {
                line = lines[i];
                break;
            }
        }

        if (line) {
            gchar **parts = g_strsplit(line, ",", 2);
            if (parts[0] && parts[0][0] != '\0') {
                gchar **key_and_time = g_strsplit(parts[0], ":", 2);
                if (key_and_time[0]) {
                    const char *hardware_key = key_and_time[0];

                    GtkEditable *editable = GTK_EDITABLE(current_editor);
                    gchar *current_text = gtk_editable_get_chars(editable, 0, -1);

                    GString *new_value = g_string_new(current_text && *current_text ? current_text : "");
                    if (new_value->len > 0)
                        g_string_append(new_value, ",");
                    g_string_append(new_value, hardware_key);

                    gtk_editable_delete_text(editable, 0, -1);
                    gint pos = 0;
                    gtk_editable_insert_text(editable, new_value->str, -1, &pos);

                    g_string_free(new_value, TRUE);
                    g_free(current_text);
                }
                g_strfreev(key_and_time);
            }
            g_strfreev(parts);
        }
        g_strfreev(lines);
    }

    g_free(contents);
}

/* File changed callback */
static void on_file_changed(GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data) {
    if (event_type == G_FILE_MONITOR_EVENT_CHANGED ||
        event_type == G_FILE_MONITOR_EVENT_CREATED) {
        parse_key_file_and_update_ui();
    }
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Keyboard Config Editor");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    store = gtk_list_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    load_config(store);

    GtkWidget *treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_box_pack_start(GTK_BOX(vbox), treeview, TRUE, TRUE, 0);

    /* Create columns */
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    // Actions Column
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "editable", TRUE, NULL);
    g_object_set_data(G_OBJECT(renderer), "column", GINT_TO_POINTER(COL_ACTIONS));
    g_signal_connect(renderer, "edited", G_CALLBACK(cell_edited_callback), store);
    g_signal_connect(renderer, "editing-started", G_CALLBACK(on_editing_started), NULL);
    column = gtk_tree_view_column_new_with_attributes("Actions", renderer, "text", COL_ACTIONS, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    // Delay Column
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "editable", TRUE, NULL);
    g_object_set_data(G_OBJECT(renderer), "column", GINT_TO_POINTER(COL_DELAY));
    g_signal_connect(renderer, "edited", G_CALLBACK(cell_edited_callback), store);
    column = gtk_tree_view_column_new_with_attributes("Delay (ms)", renderer, "text", COL_DELAY, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    // Command Column
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "editable", TRUE, NULL);
    g_object_set_data(G_OBJECT(renderer), "column", GINT_TO_POINTER(COL_COMMAND));
    g_signal_connect(renderer, "edited", G_CALLBACK(cell_edited_callback), store);
    column = gtk_tree_view_column_new_with_attributes("Command", renderer, "text", COL_COMMAND, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *add_button = gtk_button_new_with_label("Add Entry");
    g_signal_connect(add_button, "clicked", G_CALLBACK(on_add_entry_button_clicked), store);
    gtk_box_pack_start(GTK_BOX(hbox), add_button, FALSE, FALSE, 0);

    GtkWidget *delete_button = gtk_button_new_with_label("Delete Entry");
    g_signal_connect(delete_button, "clicked", G_CALLBACK(on_delete_entry_button_clicked), treeview);
    gtk_box_pack_start(GTK_BOX(hbox), delete_button, FALSE, FALSE, 0);

    GtkWidget *save_button = gtk_button_new_with_label("Save");
    g_signal_connect(save_button, "clicked", G_CALLBACK(on_save_button_clicked), store);
    gtk_box_pack_start(GTK_BOX(hbox), save_button, FALSE, FALSE, 0);

    log_label = gtk_label_new("Waiting for key data...");
    gtk_box_pack_start(GTK_BOX(vbox), log_label, FALSE, FALSE, 5);

    // File monitor
    GFile *file = g_file_new_for_path(KEY_FILE);
    GError *error = NULL;
    GFileMonitor *monitor = g_file_monitor_file(file, G_FILE_MONITOR_NONE, NULL, &error);
    if (!monitor) {
        g_warning("Could not monitor %s: %s", KEY_FILE, error->message);
        g_clear_error(&error);
    } else {
        g_signal_connect(monitor, "changed", G_CALLBACK(on_file_changed), NULL);
    }
    g_object_unref(file);

    parse_key_file_and_update_ui();

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
