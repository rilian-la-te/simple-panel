/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_APP_SYSTEM_H__
#define __SHELL_APP_SYSTEM_H__

#include <gio/gio.h>
#include <clutter/clutter.h>
#include <meta/window.h>

#include "shell-app.h"

#define SHELL_TYPE_APP_SYSTEM                 (shell_app_system_get_type ())
#define SHELL_APP_SYSTEM(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_APP_SYSTEM, ShellAppSystem))
#define SHELL_APP_SYSTEM_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_APP_SYSTEM, ShellAppSystemClass))
#define SHELL_IS_APP_SYSTEM(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_APP_SYSTEM))
#define SHELL_IS_APP_SYSTEM_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_APP_SYSTEM))
#define SHELL_APP_SYSTEM_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_APP_SYSTEM, ShellAppSystemClass))

typedef struct _ShellAppSystem ShellAppSystem;
typedef struct _ShellAppSystemClass ShellAppSystemClass;
typedef struct _ShellAppSystemPrivate ShellAppSystemPrivate;

struct _ShellAppSystem
{
  GObject parent;

  ShellAppSystemPrivate *priv;
};

struct _ShellAppSystemClass
{
  GObjectClass parent_class;

  void (*installed_changed)(ShellAppSystem *appsys, gpointer user_data);
  void (*favorites_changed)(ShellAppSystem *appsys, gpointer user_data);
};

GType           shell_app_system_get_type    (void) G_GNUC_CONST;
ShellAppSystem *shell_app_system_get_default (void);

ShellApp       *shell_app_system_lookup_app                   (ShellAppSystem  *system,
                                                               const char      *id);
ShellApp       *shell_app_system_lookup_heuristic_basename    (ShellAppSystem  *system,
                                                               const char      *id);

ShellApp       *shell_app_system_lookup_startup_wmclass       (ShellAppSystem *system,
                                                               const char     *wmclass);
ShellApp       *shell_app_system_lookup_desktop_wmclass       (ShellAppSystem *system,
                                                               const char     *wmclass);

GSList         *shell_app_system_get_running               (ShellAppSystem  *self);

#endif /* __SHELL_APP_SYSTEM_H__ */
