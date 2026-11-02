# Qt frontend: desktop checklists

## Current batch: real DCC runtime regression

The new `qt-dcc-runtime` test runs in native Linux plugin-enabled Qt builds:

```sh
meson test -C build-qt-runtime qt-dcc-runtime --print-errorlogs
```

See README.md for build setup. It requires permission for localhost sockets and
checks real bytes through the Qt executable/C engine: normal/passive transfers,
explicit receive destination, both resume directions, outgoing/passive/incoming
chat, queued decline/abort, active cancellation, removal and stale IDs. No public
IRC server or user configuration is used. This batch is automated; remaining
interactive chooser/offer and platform checks are still listed below.

## Previous batch: native alert categories

1. Preferences > Alerts: test notification, tray, taskbar and beep switches for
   channel messages, private messages and highlights independently. Save/restart
   to check native configuration persistence. Enable the notification master too.
2. Disable desktop notifications but enable a tray category. Background messages
   in that category should blink the tray without notifications. Continuous
   messages must not restart its blink phase; reading clears the indication.
3. Use per-conversation overrides: notification Off with tray On must still
   permit tray activity. Notification On retains its explicit master override.
4. Test away omission, focused-window omission, and notifications restricted to
   hidden/minimized windows. Use both main and detached conversations; taskbar
   attention should target the conversation's window. Beeps should obey the core
   focus/away rules and selected sound event.
5. Check behavior with no system tray/notification daemon. Windows must remain
   recoverable; unsupported desktop integration should not affect chat delivery.

## Previous batch: default keyboard bindings

1. With an isolated configuration containing no keybindings.conf, test Page
   Up/Down, Ctrl+Home/End, Shift+Up/Down and Shift+Page Up/Down in message input.
   They should scroll chat or change selected users as in the original map.
2. Try Ctrl+Page Up/Down, Alt+1–9, Alt+Left/Right and Ctrl+Shift+Page Up/Down
   across several server families. Check forward/reverse nick completion with
   Tab, Shift+Tab and Ctrl+Tab.
3. Use IRC formatting shortcuts on selected text; the text must remain. Check
   history and Auto Replace still work, including Return submitting once.
4. Open Keyboard Shortcuts: all 39 implicit defaults should be visible. Save
   and restart to check equivalent behavior with the resulting native file.
   Existing custom/empty files must not be replaced by defaults.
5. Without an explicit binding file, change command prefix and run GUI APPLY.
   F9 should still toggle the menu using the new prefix. Ctrl+F9 remains the
   independent menu recovery shortcut.

## Previous batch: nickname colors and indentation

1. Preferences > Appearance: enable Colored nicknames and receive new channel
   messages/actions from several users. Nick colors should follow the original
   ZoiteChat scheme and configured palette. Old transcript lines should not change.
2. Preferences > Interface: enable Color nicknames in user list. Check main and
   detached user lists; the colors should correspond to the core's nick colors.
   Disable it while keeping chat colors enabled to check the options are separate.
3. Mark a user away, change their nickname and update the palette. Away styling
   must override nick color; known selections should remain intact after refresh.
4. Toggle Sort Users by Privilege with detached windows open and check their
   lists change order too. Save/restart to verify both native color preferences.

5. In Appearance enable Indent nicknames, try several maximum column widths,
   and receive messages from short/long nicknames. Check nickname ends and wrapped
   body text align, with timestamps enabled and disabled. Try custom event text
   that contains separator markers, links and multiple lines.
6. Change fonts and indentation settings with retained history and detached
   windows. Text selection, links, markers and exported text should remain intact.

## Previous batch: command-prefix compatibility

1. Preferences > Input Box: change the command prefix to `!`. Type `!j` and
   complete it; check `!JOIN ` appears. Try normal text beginning with `/` and
   commands beginning with `!` in both main and detached inputs.
2. Use menus to join, leave, query, WHOIS, moderate and change away state. They
   must continue to execute commands under the custom prefix.
3. Check Auto Replace leaves `!command` input alone. Preview a multiline paste:
   its checkbox should name the new prefix; literal mode must still send literal
   text. In command mode, `!echo` executes and `/echo` is ordinary text.
4. Run Command bindings should use their configured text unchanged. Update any
   command prefixes intentionally in the binding editor. Doubling the configured
   prefix should retain the native parser's literal escape behavior.
5. Invalid empty/multiple/non-ASCII prefixes should be rejected by Preferences.
   Save/restart to verify persistence. Restore `/` when finished if desired.

## Previous batch: chat background images

1. Preferences > Appearance: browse to a local image, check preview and OK.
   Check it tiles behind chat in main and detached windows. Transparent images
   should blend with the configured background color.
2. Change the image, then Cancel. The previous background must remain. Clear
   and OK should restore plain background color. Restart to check persistence.
3. Try unreadable/invalid files and dimensions above 4096 pixels. OK should show
   an error while leaving other pending settings unapplied and the dialog usable.
4. Edit the image externally and use `/GUI APPLY`. Remove the file and apply
   again: preserve the previous valid image. Restart with a missing file: use the
   normal background color and show a status error.
5. Check text selection, reverse/color formatting, copying and exported text.
   Image data must not appear in copied/exported transcripts. The input should
   retain a plain background even when sharing chat font/colors.

## Previous batch: detached controls and placement

1. Detach a joined channel. Toggle a supported simple mode and confirm the
   control waits for the server reply. Check key presence and user limit updates,
   and disable/hide unadvertised modes. Disconnect and check controls disable.
2. Enable User List Buttons in the detached View menu. Select users, run custom
   commands and check the right nick/channel receives them. Edit the button
   configuration while the window is open and check it refreshes.
3. Detach a query and enable Dialog Buttons. Its buttons should use the query's
   target even after selecting another conversation in the main window. Channel
   modes and user panels should stay hidden for queries.
4. Hide/show the modes and button panels independently in each window. Change
   selection and connection state to check button enablement updates.
5. Resize and position a detached window and its splitter, attach it, then detach
   the same conversation again. Repeat after restarting: placement should return
   once the same session exists and is detached. It does not reopen sessions.

## Previous batch: completion ordering and thresholds

1. Preferences > Input Box: try Alphabetical and Most recently spoken. Have
   several matching nicknames send messages, then start a new completion and
   check the order. Your own nickname should come after other matches in recent
   speaker order.
2. Set completion amount to 1, type a prefix with several matches, and press Tab.
   Check the common prefix expands and candidates appear in that conversation.
   Repeated Tab should list again until the prefix narrows to a single match.
3. Set the amount to zero: ambiguous input should always list. Set it above the
   match count: Tab and Shift+Tab should cycle directly. Try Unicode names and
   preserve text after the cursor.
4. Repeat in detached windows. Change `/set completion_amount` or
   `/set completion_sort` and run `/GUI APPLY`; check all inputs use the change.
5. On a server using RFC1459 case mapping, test matching equivalent bracket
   characters (`[` and `{`). Save/restart to verify native preference persistence.

## Previous batch: keyboard compatibility

1. Complete `/j` with Tab, then Shift+Tab; check core and custom/plugin command
   names appear and use a trailing space. Complete `#channel` in `/join` or
   `/msg` arguments. Try in both main and detached conversations.
2. Load a legacy modifier/key binding file in an isolated configuration. Check
   combinations of C/A/S and None. Mix modern ACCEL records with old records;
   saving the editor should convert old records without losing comments/data.
3. Bind Move tab family left/right. With two server connections and several
   conversations per server, check each family moves as a group and retains its
   internal order. Check ordinary tab movement too.
4. Bind Push input line into history: the draft should clear without sending,
   and Up should recall it. Bind Check For Replace to Space/Return: replacement
   should occur once and the original space or submit should still work.
5. Try malformed/truncated files and external edits while the editor is open.
   Failed reload/save should retain the previous usable configuration.

## Previous batch: detached conversation tools

1. Detach two conversations. In each, try custom keyboard shortcuts, literal
   and regex search, export, markers, user moderation and custom user menus.
   Actions should use their source conversation regardless of the main tab.
2. Paste several lines into a detached input. Check preview, Cancel, paced
   delivery and literal handling of `/quit`. Switch the main tab during preview;
   delivery must stay with the detached source. Closing or rejoining the source
   must stop queued lines and multi-command keyboard bindings.
3. Open channel settings, bans, conversation options, raw logs and global tools
   from a detached window. Check settings and custom menus remain accessible.
4. Run `/GUI MENU`, `/GUI FOCUS`, and `/GUI ICONIFY` in detached windows. MENU
   must affect only that window; Ctrl+F9 should recover its hidden menu. Focus
   the main window again and check commands use its visible conversation.
5. Edit shortcuts while windows are open; saving or `/GUI APPLY` should reload
   detached inputs too. Close a session from a detached shortcut and check the
   window disappears without closing other sessions or executing later commands.

## Previous batch: appearance and input settings

1. Preferences > Appearance: set `[%H:%M:%S] ` as timestamp format. Check the
   preview and new incoming messages. Old transcript text should stay intact.
   Empty formats and formats that produce newlines should be rejected.
2. Toggle marker visibility, set a marker, switch conversations and toggle back.
   Check the existing marker position survives. Try optional nickname, modes
   and user counts in titles; counts should update as users join/leave.
3. Try opacity and compact user lists. Enable automatic copying, select chat
   text and release the mouse; the clipboard should contain plain selected text.
4. Preferences > Input Box: try chat font/colors, the nickname label, completion
   on/off, suffix `,` (and an empty suffix), and Auto Replace on/off. Completion
   in the middle of a line should not append the suffix.
5. Repeat in detached windows; save, reopen and restart to check persistence.
   Cancel should discard edits. Opacity may be ignored by the window system.

## Previous batch: file menu and preferences

1. Compare ZoiteChat and its New submenu to the original: Network List, New
   (Server Tab, Channel Tab, Server Window, Channel Window), Load Plugin or
   Script, Detach/Attach, Close, Quit. Check the detached window's ZoiteChat menu
   too. Load should be disabled in builds without plugin support.
2. Open Preferences and visit all categories. Change quit/part/away text,
   highlights, reconnect delay, proxy configuration, logging and DCC settings.
   OK should persist them; Cancel should discard changes. Reopen and restart
   to check persistence. Reconnect before checking changed connection settings.
3. Try an inverted DCC port range, an oversized text setting and a failed save.
   The error should leave the dialog editable and other settings unapplied.
4. Change a setting with `/set` while Preferences is open, then edit that same
   setting in Preferences. OK should report a conflict, preserving `/set`.
5. Check font/wrapping, panels, alerts, colors and sounds. Colors/Sounds editors
   have independent Save/Cancel buttons.

## Previous batch: detached conversations

1. Right-click a conversation and Detach Conversation. Check live messages,
   input history, nick completion, topic editing and user query/WHOIS actions.
2. Detach several conversations. Send in each and check the correct destination.
   Selecting a detached conversation in the main list should raise its window.
3. Type a draft, close the window and check it reattaches with draft/history.
   Also try `/GUI DETACH`, `/GUI ATTACH`, and `/GUI FOCUS`.
4. Close a detached session with `/close`; check its window disappears and other
   conversations keep working. Quit with several windows open.

## Previous batch: input key bindings

1. Open Settings > Keyboard Shortcuts. Add `F10`, Run Command, Data 1
   `/echo key-test`, save and press F10 in the message input. Check the original
   conversation receives the command; the input draft should remain unchanged.
2. Add `<Alt>x`, Insert in Buffer, Data 1 `example`. Check insertion replaces the
   input selection. Try Set Buffer, history, completion, scrolling and Change Page.
3. Put `/echo one\n/echo two` into Run Command Data 1 and check both commands.
   A command that closes its source should stop remaining lines. Holding the key
   should not repeatedly send commands.
4. Reorder duplicate keys, Reload and Cancel. Unsupported actions/keys should
   remain visible and preserved. Try an invalid multiline field and an external
   edit while the editor is open: Save must reject rather than overwrite it.
5. Restart to check persistence. Bindings should be scoped to message input and
   must not execute while typing in another dialog. Defaults should still work
   when no supported custom binding handles the key.

## Previous batch: custom tray icons

1. With a usable tray, run `/tray -f /path/to/icon.png` and check the fixed image.
   `/tray -f` should restore the normal icon.
2. Run `/tray -f 500 /path/to/first.png /path/to/second.png` and check alternation.
   Omit the second filename to alternate with the normal icon. `-1` should select
   the native 500 ms default interval.
3. Try a missing/invalid image, including an invalid second frame. The current
   valid state should remain and an inline status should explain the failure.
4. Disable the tray, then re-enable it. Animation should stay idle while no usable
   tray exists. Normal unread/reset events should cancel custom flashing.

## Previous batch: per-conversation options

1. In a channel or private query, open Window > Channel > Conversation Options.
   Check Default, On and Off for all options. Cancel must discard drafts.
2. Apply Hide joins and parts or Strip message colors and receive relevant new
   messages. Check only the selected conversation changes. Existing history stays.
3. Toggle disk logging/scrollback, then check the core's expected files and logging
   behavior when convenient. Named-network options should persist after normal
   shutdown/restart; unnamed-network options are marked as session-only.
4. Set desktop notifications Off for one conversation and On for another. On may
   override the global notification switch; normal focus/cooldown rules remain.
   Check beep, taskbar flash and tray activity overrides separately.
5. Change an option using `/chanopt` while an editor is open. Apply should reject
   its stale snapshot; Reload should retrieve the actual state.
6. Leave/rejoin or close the original conversation while an editor is open. Its
   old draft must not apply to a reused conversation, even at the same address.

## Previous batch: inline channel modes

1. Join a channel and check the mode bar below the topic. Confirm it reflects
   current modes; flag controls absent from the server's advertised simple modes
   should be hidden.
2. Toggle a mode with sufficient permissions. The button should retain confirmed
   state while awaiting the server and update when the server confirms. Without
   permission, check the normal server error and unchanged confirmed state.
3. Change modes from another client. Mode, Key presence and Limit should update.
   The bar should not display the actual channel key.
4. Click Key or Limit to open Channel Settings. Check the existing edit/clear
   actions and confirmed values after replies.
5. Switch among channels, server tabs and queries. The bar should follow the
   current channel and hide elsewhere. Toggle View > Channel Modes and restart
   to check that visibility persists.

## Previous batch: character chart

1. Open Window > Character Chart. Browse several ranges and pages, including
   Latin Extended, Greek, Cyrillic and Emoji. Hover/select a cell and check its
   Unicode code point.
2. Look up `U+1F600`, `0x0041` and `03B1`. Enter should only locate the character;
   Insert or double-click should add it to the current input draft without sending.
3. Select part of an input draft before Insert and check replacement and cursor
   placement, including emoji. Switch conversations while the chart remains open;
   insertion should follow the current conversation.
4. Check Copy. Invalid `110000`, surrogate `D800` and control `000A` should be
   rejected. An unavailable glyph may depend on fonts, but copy should retain the
   actual character. Close the final conversation and try Insert: it should show
   a local status and send nothing.

## Previous batch: direct-chat manager

These checks can stay deferred until a peer or loopback integration setup is
available; current automated checks exercise adapter and UI behavior.

1. Open Window > Direct Chat. Start an offer through New Direct Chat or a saved
   Chat button. Check outgoing queued/connecting/active states and byte counters.
2. Receive an offer and test acceptance and rejection. Outgoing queued offers
   must not offer Accept. File-only Save As and Resume controls must not appear.
3. Once active, Open Conversation should open/focus the peer's query. Exchange
   messages in both directions and verify the connection uses the core DCC path.
4. Disconnect queued, connecting and active chats; remove terminal entries.
   Confirm that the file-transfer window never mixes in chat entries.
5. Remove an offer while its deferred prompt is pending and check it cannot
   affect a later chat. Try passive offers through the core commands when a peer
   is available. Existing normal query behavior resumes after the direct chat ends.

## Previous batch: DCC file-transfer interface

These checks can remain deferred; no second IRC client is required for the
current automated adapter/UI suites.

1. Open Window > File Transfers and check an empty view. Use Send File or
   `/send nick` to choose files. Cancel should send no offer. Send buttons in
   custom menus should now be enabled when a recipient is selected.
2. When a peer is available later, send and receive a small file. Check queued,
   connecting, active and done states, progress, byte counts and rate. Verify the
   received bytes independently.
3. Test incoming Accept and Save As, including cancelling a prompt and removing
   the offer while a prompt is open. A stale prompt must not affect another
   transfer. Core auto-receive settings should retain their existing behavior.
4. With a partial destination file, test Resume. A second Accept/Resume request
   must be disabled while negotiation is pending. Check resume negotiation and
   resulting bytes when a peer is available.
5. Abort a queued/active transfer and remove a terminal entry. Changing the
   selected row during Save As must retain the original transfer identity.
6. Open a send picker, close its original conversation and create another tab.
   Selecting a file must not send through the new conversation.

## Previous batch: plugins and scripts manager

1. Open Window > Plugins and Scripts. Check filtering, columns, Refresh and
   selection retention. Built-in entries should not offer Unload/Reload.
2. In a plugin-enabled build, load a compatible non-GTK addon, then unload/reload
   it. The list should follow both manager operations and typed core commands.
   Native loader failures or refusal to unload should be shown in the manager.
3. With a matching language plugin loaded, load and unload a script with spaces
   in its path. Script diagnostics should appear in the source conversation.
4. Open Load, switch conversations or close the original one before choosing a
   file. The operation must keep the original context or decline if it closed.
5. In a plugin-disabled build, confirm the manager explains that support is
   disabled and does not offer load/unload/reload operations.

## Previous batch: chat color editor

1. Open Preferences > Configure Chat Colors. Edit hex values, use Choose, and
   check the swatches. Changes should remain drafts until Save.
2. Save changes to IRC color 18, Chat background, Marker, Mention and Away user.
   Check subsequent messages, the chat view, a marker, background-conversation
   activity, and an away nick. Restart and check persistence.
3. Try an invalid hex value. Save should show an inline error and leave the file
   and live palette unchanged. A failed file write should also keep the editor
   open without applying changes.
4. Check Default for Selected, Defaults for All, Reload and Cancel. Cancel should
   preserve the saved palette. Existing rendered message colors remain unchanged.
5. Edit the palette externally while the editor is open. Save should reject the
   stale draft until Reload. Untouched comments, unknown settings and native
   16-bit RGB precision should survive edits.

## Previous batch: native color palette

1. Receive ordinary channel messages and events. Color slots 16–31 in the core's
   default formats should now render rather than falling back to plain text.
2. Start with an existing ZoiteChat `colors.conf` in the selected configuration
   directory. Check custom IRC colors, chat foreground/background and selection
   colors. Text Events previews should use the same palette.
3. Try literal formatting or event formats with colors 18, 31 and 99. Code 99
   should restore default colors. Higher native color codes should wrap as in the
   original frontend.
4. Change the palette file externally, run `/gui apply`, and check subsequent
   messages and previews use it. Already-rendered text retains explicit colors;
   the background changes immediately.
5. A missing file or malformed individual entries should retain defaults and
   should not prevent startup. The Qt frontend must not rewrite the palette file.

## Previous batch: GUI commands and partial clearing

1. In a conversation with several lines, run `/clear 2` and check only the oldest
   two lines disappear. `/clear -2` should remove only the newest two. Large
   counts and `/clear` should safely clear all retained text.
2. Set a marker, remove lines before or after it, and check it stays on its line.
   Remove the marked line and check the marker disappears. Repeat on a background
   conversation when convenient.
3. Toggle View > Menu Bar and restore it with Ctrl+F9. Run `/gui menu toggle` and
   check the same action reflects its state. Verify visibility after restarting.
4. Try `/gui iconify`, then restore through the desktop. `/gui hide` should hide
   to the tray when available and otherwise minimize. `/gui show` and scripted
   `/gui focus` should restore the window; FOCUS selects its command context.
5. Try `/gui apply` after changing custom menus/buttons. It should refresh the Qt
   controls. ATTACH/DETACH should report that separate windows remain unsupported.

## Previous batch: filtered scrollback

1. Open Find (Ctrl+F), enable Regex and search for `hello|world`. Check Next,
   Previous, wrapping and Match case. Invalid `[` should produce an inline error.
2. Choose Show Matching Lines and check the `(lastlog)` conversation contains
   matching lines with original colors, links and timestamps. Source history must
   remain unchanged, and generated timestamps should not produce matches.
3. Try `/lastlog -h -- word`, `/lastlog -m -- Word` and
   `/lastlog -h -r -- hello|world`. Check highlighted matches, case sensitivity,
   no matches, an empty buffer and invalid regexes.
4. Type another search in the `(lastlog)` input to repeat against its source.
   Close the source conversation and repeat: the core should safely decline it.
5. Check the search bar at a narrow window size and confirm filtered results
   respect the configured scrollback limit.

## Previous batch: connection status

1. Connect to a server and watch the status bar for Connecting, Registering and
   Connected. The progress indicator should disappear after registration.
2. After the core sends latency probes, check the Lag label. Outstanding probes
   show an elapsed-time lower bound; a reply should replace it with measured lag.
3. Switch between conversations on different servers. Status, lag and queue bytes
   must follow the active server. Disconnect should hide stale latency.
4. If an automatic reconnect is scheduled, check Waiting to reconnect and verify
   Server > Disconnect remains available to cancel it.
5. If outgoing messages accumulate, check Queue and Server > More > Clear Outgoing
   Queue. Clearing should use the active server and discard its queued messages;
   the action should disable when the queue empties. This check can wait until a
   queue occurs naturally.

## Previous batch: event sounds

1. Open Preferences > Configure Event Sounds. Filter events and assign a WAV file
   to an event. Browse, edit the path, switch rows and check the draft remains.
2. Play Selected should preview the draft without changing the live assignment.
   A missing/unreadable path should show an inline error. Audio requires the
   core's platform backend, libcanberra, or the `play` utility.
3. Save, trigger the event with another nick when convenient, and restart to
   check persistence. Test both an absolute path and a filename relative to the
   configuration's `sounds` directory.
4. Clear Selected then Cancel: the saved assignment should remain. Clear it
   again and Save: that event should no longer play a custom sound. Reload should
   discard pending edits. Restore personal assignments after testing.

## Previous batch: text event formats

1. Open Settings > Text Events. Filter for Channel Message, select it, and check
   that its format and `$1`, `$2` argument descriptions appear.
2. Change the format to `[%C04$1%O] $2`, click Preview and check colors and sample
   arguments. Receive a new channel message before Save: it should retain the
   live format. Save, then receive another message: it should use the new format.
3. Try an unavailable argument such as `$9`, a trailing `$`, and an oversized
   format. Save should show an inline error and leave all live formats unchanged.
4. Edit several rows, switch selections, filter/unfilter, and verify drafts stay
   attached to their event. Check Default for Selected, Defaults for All, Reload
   and Cancel. Restore personal formats afterward.
5. Export a draft and import it again. Matching event names should merge into the
   draft; unrelated events should remain. Import should not apply until Save.
6. Restart after saving and check persistence. Existing chat history should retain
   its original rendering; only subsequent messages should use new formats.

## Previous batch: contextual customization

Desktop checks can be deferred and performed together.

1. Edit User List Buttons and Dialog Buttons under Settings. Add an `Inspect`
   button with `echo target=%s selected=%a host=%h account=%u`. Check channel
   selections, multiple selected users, filtering and private conversations.
2. Toggle both button bars under View and check persistence after restarting.
   Save/reorder/remove definitions and confirm the bars refresh. Direct DCC
   buttons should be disabled.
3. Edit User List Popup and Channel Tab Popup. Add a nested `SUB` / `ENDSUB`
   group containing `Inspect` → `echo target=%s channel=%c`. Right-click a nick
   or conversation and find it under Custom Actions.
4. Add a URL handler with `Inspect` → `echo link=%s`. Check Open With from a
   chat link and URL Grabber entry. Normal Open and Copy should still work.
5. Run `/menu ADD "$NICK/Inspect" "echo nick=%s"` and equivalents for `$TAB`,
   `$CHAN`, `$URL` and `$TRAY`. Confirm each appears in its corresponding menu;
   remove them afterward using `/menu DEL "$NICK/Inspect"` and equivalents.
6. Open a custom popup, then switch/close its conversation or let the nick leave
   before activating it. Commands must retain their original destination or
   safely do nothing when the context has gone away.

## Previous batch: customization and dynamic menus

1. **Editor basics:** Open Settings > Auto Replace, CTCP Replies and User
   Commands. Edit cells, add/remove rows, move rows up/down, Cancel and Reload.
   Cancel should leave the core untouched; Reload should discard local edits.
   Blank names, multiline values and values beyond the displayed byte limits
   should not save. Duplicate command names should remain allowed.
2. **Auto Replace:** Add `qtteh` → `the`. In plain chat input, type `qtteh`
   followed by Space or Enter and verify it becomes `the`. `Qtteh` should not
   match, and `/echo qtteh` should stay unchanged. Remove the test rule later.
3. **Aliases:** Add a User Command named `QTTEST` with command `echo test &2`.
   Save and run `/qttest hello world`; the expanded result should appear locally.
   Add a second `QTTEST` row with `echo second %2`; both definitions should run.
   Restart and verify persistence, then remove the test entries.
4. **CTCP:** With another nick if available, add `QTTEST` with reply
   `nctcp %s QTTEST received`, save, then send yourself `CTCP QTTEST` from the
   other nick. Check the response uses the existing core behavior. Restore the
   original CTCP list after testing.
5. **Usermenu:** Open Usermenu > Edit This Menu. Add an `Echo test` row with
   `echo menu %c %n`, save and activate it on two conversations. Check the correct
   channel and nick are expanded. Add `SUB` / `ENDSUB` rows around a group and
   a `SEP` row; unmatched submenus should be rejected at Save.
6. **Preference toggle:** A Usermenu row named `TOGGLE Hide version` with
   command `irc_hide_version` should show the core's current boolean state and
   toggle it when clicked. Changes made with `/set irc_hide_version` should be
   reflected when reopening the menu. Restore the original value afterward.
7. **Dynamic menus:** Enter these commands, then use Window > QtTest:

   ```text
   /menu ADD Window/QtTest
   /menu ADD Window/QtTest/Echo "echo dynamic-menu-test"
   /menu -t0 ADD Window/QtTest/Toggle "echo toggle-on" "echo toggle-off"
   /menu -r1,One ADD Window/QtTest/One "echo radio-one"
   /menu -r0,One ADD Window/QtTest/Two "echo radio-two"
   ```

   Echo should run only on activation. Toggle should run its on/off command
   and retain state. Radio items should remain exclusive.
8. **Update/delete:** `/menu -e0 ADD Window/QtTest/Echo` should disable Echo;
   `-e1` should enable it again. `/menu DEL Window/QtTest` should remove the
   submenu and its children, without disturbing the original Window entries.
   Runtime entries should not return after restarting.
9. **Shortcuts/context:** If assigning `/menu -k<modifier>,<GDK-key>` bindings,
   test printable or function keys. Existing shortcuts must keep working if a
   dynamic binding conflicts. Switch conversations before using a custom item;
   it should use the currently active context. Deleting an entry while a menu is
   open must not execute a stale command or crash.
10. **Persistence/conflicts:** Reopen the editors and restart to verify saved
    lists retain order and content. If a list/file changes while its editor is
    open, Save should reject the stale version. Changes should not be partially
    applied after a validation or write error. Remove test customizations when
    finished. The existing core config files remain shared with GTK.

Automated checks cover actual atomic config writes in a temporary directory,
validation, duplicate aliases, stale core/file revisions, failure rollback,
command context, menu state/lifetime guards, Usermenu rendering, dynamic paths,
checkboxes/radios, deletion and shortcut conflicts, plus editor and replacement
behavior. Popup-root menus, markup and custom icons remain outside this batch.

## Previously tested batch: friends, new tabs, markers and input

1. **New server tab:** ZoiteChat > New > Server Tab (Ctrl+T) should create and
   select an unconnected server session. Connect through Network List or Quick
   Connect and verify the previous server remains independent.
2. **New channel tab:** ZoiteChat > New > Channel Tab should create a `(not joined)`
   session on the current server. Join a channel there, switch between tabs,
   and close/leave normally. Check the unused session is reused when possible.
3. **Friends CRUD:** Window > Friends List: add a nick, edit its name/network
   restrictions, cancel an edit, reject a duplicate and remove a rule. Empty
   network scope means all networks. Also try Add to Friends List from a
   nickname's context menu.
4. **Presence:** With another nick available, check online/offline transitions,
   network labels and last-seen timestamps. Filter and sort the list. Open an
   online friend dialog and check it uses the correct network. A stale/offline
   row should not open a dialog. Presence can initially be Unknown and depends
   on the engine's MONITOR/WATCH/ISON tracking interval.
5. **Friends persistence:** Add a rule restricted to a saved network name, quit
   and restart, and confirm it remains with that scope. `/notify nick` should add a missing nick or remove an existing nick and
   update the window. Remove test rules afterward.
6. **Markers:** Press Ctrl+M, receive more lines, then Ctrl+Shift+M. The view
   should return to the highlighted marker row. Switch away/back to check the
   automatic last-visible-line marker. Each conversation should keep its own
   marker. Clear Text or enough scrollback trimming should remove the marker
   safely; Save Text should contain no extra marker text.
7. **Literal multiline paste:** Copy a few lines including text beginning with
   `/`, then Ctrl+V into a draft. Check the preview, edit it, and send. With the
   commands checkbox off, the slash line should appear as ordinary text. Use
   a harmless `/echo paste-test` line for this check.
8. **Command paste:** Explicitly enable command interpretation for harmless
   `/echo` lines. They should run as commands. Blank lines are skipped; more
   than 100 lines or 64 KiB should not be sendable in one batch.
9. **Paste cancellation/context:** Cancel and verify the existing draft remains.
   Repeat while switching conversations; accepted lines should stay on the
   original destination. Close/disconnect/leave that destination partway through
   a longer paste and confirm remaining lines stop. A second paste should not
   interleave with an active paste queue.
10. **Paste entry points:** Try keyboard paste, right-click Paste and a
    multiline text drop. On desktops with primary selection, middle-click
    multiline paste should also preview. Single-line paste should still insert
    immediately without sending.
11. **Formatting keys:** Try Ctrl+B/K/I/U/R/O while typing, then select text and
    apply formatting. Send it to check IRC rendering; type colour digits after
    Ctrl+K. Verify history, completion, copying and menu shortcuts still work.

Automated checks cover the actual Friends List C adapter with a fake server,
friend validation, online query guards, new-tab routing, the friend editor,
marker trimming, paste preview/limits, literal versus command routing,
destination changes, draft preservation, formatting controls and menu order.

## Previously tested change: original ZoiteChat menu layout

The menu bar should read **ZoiteChat, View, Server, Usermenu, Settings, Window, Help**.
No new features were added. Unsupported original entries remain omitted.

1. **ZoiteChat:** Network List first, then New, Close and Quit, with separators.
   Ctrl+S should open Network List, Ctrl+W close the current conversation,
   and Ctrl+Q quit even when close-to-tray is enabled.
2. **View:** Topic Bar and User List should toggle the existing panels.
   Ctrl+F7 toggles User List. Channel Switcher and Text Display submenus
   retain the existing switcher visibility, follow, jump and zoom controls.
3. **Server:** Check Disconnect, Reconnect, Join a Channel and Channel List.
   Marked Away (Alt+A) should be checked only when the server reports away;
   toggling it should set away/back without a dialog. Server > More keeps
   Quick Connect, Change Nickname and Away Message.
4. **Settings:** Preferences opens the existing display settings. User List
   and Desktop submenus contain the previously tested Qt-specific settings.
   Existing saved preferences should be unchanged.
5. **Window:** Ban List, Ignore List, Raw Log and URL Grabber should open the
   existing windows. Copy Selection uses Ctrl+Shift+C. Clear Text and Save
   Text affect the current chat. Ctrl+S should no longer export chat.
6. **Search/navigation:** Window > Search contains Search Text, Search Next
   and Search Previous. Check Ctrl+F, Ctrl+G and Ctrl+Shift+G; F3/Shift+F3
   remain aliases. Window > Channel and Navigation retain existing actions;
   Alt+backtick visits the next unread conversation, while Alt+A is Marked Away.
7. **Help:** About should open the existing About dialog. Restart and verify
   all panel, desktop and sorting preferences still apply.

Automated checks assert the top-level order, supported action groups, labels,
original shortcuts, absence of duplicate shortcuts and the away toggle's
checked state and command behavior.

Topic-bar check: select/copy the current topic, edit it directly and press
Enter. Confirm it changes after the server accepts it. Test an empty topic
(clear), a topic beginning with `#`, and a rejected change without permissions.
The last confirmed topic should remain until a successful reply arrives.
Switch conversations and verify their own topics return; private/disconnected
conversations should keep the field read-only. An empty topic shows a hint.

## Previously tested batch: user details, desktop integration and URL history

1. **User sorting:** Toggle Settings > User List > Sort Users by Privilege. Privilege order
   should follow the server's advertised ranks; alphabetical order should
   ignore `@`/`+` display prefixes. Selection should survive sort changes.
2. **User filtering:** Filter by nickname and any known host, real name or
   account. Check the visible/total and away counts. Select several users,
   filter some out, and copy/insert the visible selection. Hidden selected
   users should not receive nickname actions; clearing the filter restores
   their selection.
3. **Away display:** Have another nick set away/back. When the server reports
   those changes, its row should become italic/dim and return to normal, and
   the away count should update. The client relies on the engine's existing
   away tracking and the server's away-notify/WHO support.
4. **Tooltips/details:** Hover a user, then open User Details from its context
   menu. Check known host, real name, account and server information. Unknown
   fields should be labeled Unknown. Request WHOIS; normal results appear in
   chat and known metadata may update in the dialog. Leave the dialog open
   while that user leaves; its action should stop working and cached data
   should remain readable.
5. **Tray controls:** With a desktop tray available, use Settings > Desktop > Show Tray
   Icon, click the icon to show/hide, and use its Show / Hide menu. Window size
   and maximization should survive restoring. Disabling the tray while hidden
   should restore the window. A desktop without tray support must remain usable.
6. **Close-to-tray:** Enable Settings > Desktop > Close Window to Tray and close the main
   window. It should hide while staying connected, and restore through the
   tray. ZoiteChat > Quit and tray Quit must still exit. With the tray disabled
   or unavailable, closing the window must exit normally.
7. **Notifications:** Enable Settings > Desktop > Notify for Mentions and Private Messages,
   focus another application, and send yourself a mention/private message from
   another nick. Check the unread icon and notification/attention behavior.
   Ordinary channel messages and replayed scrollback should not notify.
   Active application dialogs should count as focused, too.
8. **Notification settings:** Message previews should be absent by default.
   Enable Include Message Previews and test again; disable notifications and
   check popups stop. Notifications are throttled per conversation for ten
   seconds and are emitted for new unread activity. Clicking a notification
   or the tray's latest-notification action should restore the right existing
   conversation, without selecting a closed/reused one.
9. **Desktop persistence:** Quit/restart after changing tray, notification,
   preview, close-to-tray and user-sorting options. Check they return. Actual
   tray availability and OS notification delivery depend on the desktop.
10. **URL capture:** Open Window > URL Grabber, send/receive HTTP(S), `www.` or
    IRC(S) links, then filter and sort. Repeat a URL to check deduplication.
    If no links are captured, check `/set url_grabber on`. Close/reopen the
    window and confirm recent captures remain.
11. **URL actions:** Copy multiple selected URLs; save all while a filter is
    active and check the UTF-8 text file still contains all retained URLs.
    Open a normal URL to check the desktop handler. Unsupported schemes
    should be refused by Open rather than launching another application.
12. **URL clear/lifecycle:** Clear All, then send a previously captured URL
    and confirm it can be captured again. Existing selections should survive
    newly arriving links. History is bounded to 1,000 URLs and is not restored
    after quitting the application.

Automated checks cover user metadata, escaped tooltips, sorting/filtering,
visible-only action targets, details/WHOIS guards, desktop settings persistence,
notification decisions/cooldowns, tray-unavailable behavior, safe URL schemes,
capture limits, copy, clear and export. Native tray clicks and OS notification
delivery require the desktop checks above.

## Previously tested batch: channel moderation and user actions

Use a test channel where you have operator privileges and, if available, a
second nick you control. Server permissions still apply; a rejected action
should produce the normal server error in chat. DCC remains deferred.

1. **Mode display:** Open Window > Channel > Channel Settings. Confirm the mode
   checkboxes match `/mode #channel`. Unsupported modes should be disabled.
   Change a mode from another client and confirm the dialog updates.
2. **Mode changes:** Toggle invite-only, moderated, secret/private, no outside
   messages and topic protection as supported. The checkbox should reflect
   the server's confirmation rather than assuming the request succeeded.
   Restore the channel's original settings afterward.
3. **Key and limit:** Set/remove a channel key and user limit. Confirm the
   server accepts each change. The new-key field should be masked and clear
   after sending; the summary should not display the key. Refresh should
   query the server's modes.
4. **Bans:** Open Window > Ban List. Add a test mask
   such as `testnick!*@*`, wait for confirmation in chat, and Refresh. Check
   mask, setter and time; filter/sort rows, copy selected masks, select several
   and remove them. Refresh should confirm removal. An empty list should also
   complete normally.
5. **Exceptions:** If advertised by the server, switch to Ban exceptions and
   repeat add/remove/refresh. Unadvertised lists should be absent. Opening a
   second channel's manager should retain independent results.
6. **Operator/voice:** Right-click your second nick and give/remove voice and
   operator status. Use Ctrl/Shift selection to act on multiple test users.
   Actions should use raw nicknames rather than their `@`/`+` display prefixes.
7. **Kick/ban:** Kick a test nick with a reason, then rejoin it. Try Ban and
   Kick and Ban; verify the core-generated mask in the ban manager and remove
   it when finished. Canceling a reason dialog should send nothing.
8. **Invitations:** Window > Channel > Invite User invites a named user into the
   current channel. The nickname menu's Invite to Channel prompts for a
   destination channel. Check the correct network and users receive invitations.
9. **Nickname utilities:** Select one or more users, then Copy Nicknames and
   Insert Nicknames. Copy should omit privilege prefixes; Insert should keep
   your surrounding draft and cursor position.
10. **Changing context:** Open a kick/invite prompt, then switch channels or
    have one selected user leave. Accepted actions should retain the original
    channel and skip departed users. Leaving/disconnecting/closing a channel
    should close its settings and list manager. A stale mask or user prompt
    must not affect another channel after rejoining or reconnecting.
11. **Permission errors:** Without operator privileges, try a mode or ban
    request. Check the server error appears in chat and no false confirmed
    state appears. A denied list request unlocks after a 15-second timeout.

Automated checks include the actual C moderation adapter against a fake server
transport, plus Qt dialog, menu and command-routing tests. They cover mode
support, key/limit arguments, mask validation, independent channels, stale
session pointers, user departures and server-confirmed controls. Actual server
permissions and protocol replies still need the desktop checks above.

## Previously tested batch: IRC tools

Start with Server > Channel List and the Window menu. Use two networks if convenient to check that
their channel browsers and raw logs remain separate. DCC remains deferred.

1. **Channel browser:** Open Server > Channel List (Ctrl+Shift+L), or type
   `/list`. Wait for completion, sort by user count, filter names/topics, and
   raise the minimum user count. Check sorting is numeric and topic text is plain.
2. **Refresh/filter/join:** Enter a server LIST filter such as `#linux*` and
   Refresh (support depends on the server). Check Refresh retains this filter.
   Join a result by button and double-click; the correct network should receive
   the join. Close and reopen the browser while results are arriving.
3. **Browser lifecycle:** Disconnect/reconnect with the browser open, refresh
   again, and close the last conversation for that network. Its browser should
   close without affecting another network's results or crashing.
4. **Raw log:** Open Window > Raw Log, send a message or `/ping` request,
   and confirm outgoing `>>` and incoming `<<` lines appear. Try Pause capture,
   resume, Clear, copy selected text, and Save As. Confirm Unicode survives
   export. Log capture begins at first opening; reopening retains recent data.
5. **Raw log lifecycle:** Switch networks and verify independent logs. Close
   the last conversation for a network with its log open; it should disappear
   safely. Reconnecting should work. Common authentication commands should be
   redacted if they occur during capture; no need to send credentials just to test.
6. **Ignore rules:** Window > Ignore List, add a mask such as `testnick!*@*`,
   choose message types, edit/rename it, cancel an edit, and remove it. Empty
   masks, duplicate masks and selecting no message types should be rejected.
   With a second nick available, verify the selected traffic is ignored and
   an Exception rule permits matching traffic of the selected types.
7. **Ignore persistence:** Save a normal rule and a temporary rule, quit and
   restart. The normal rule should remain; the temporary one should disappear.
   `/ignore anothernick!*@* PRIV` and `/unignore anothernick!*@*` should update
   the open manager. Remove test rules when finished.
8. **Basic prompts:** Run these commands, accept and cancel each dialog, and
   confirm results appear only as local `echo` text:

   ```text
   /getstr hello echo Enter-text
   /getint 7 echo Enter-number
   /getbool echo Question Continue?
   /getfile echo Choose-file
   /getfile -multi echo Choose-files
   /getfile -folder echo Choose-folder
   /getfile -save echo Save-file
   ```

   GETBOOL reports `1` for Yes and `0` for No/close. GETSTR/GETINT cancel
   without running their command. File requests run the command for each
   selected path and once more without an argument on completion or cancel,
   following the existing C command's contract. A save-file prompt selects
   a path; these `echo` examples do not write a file. Try Unicode text, multiple
   files and canceling while another conversation is active.

## Previously tested everyday-use batch

Build and launch:

```sh
meson compile -C build-qt
./build-qt/src/fe-qt/zoitechat-qt
```

Use two channels and a private conversation for the checks below. DCC testing
is deferred. The earlier desktop checks for message history, completion, user
selection, private-chat activation and WHOIS have passed; exercise them again
as part of normal chatting if convenient.

## Combined checklist

1. **Connection controls:** Disconnect from Server, then Reconnect. Confirm the
   status bar follows the connection and shows your server and nickname.
   It refreshes approximately once per second. Repeat on another network if
   available; only the selected conversation's connection should be affected.
2. **Nickname:** Change Nickname through Server. Confirm the server accepts it
   and the status bar updates, or shows the server's rejection in chat.
3. **Away/back:** Use Server > More > Away Message to set a reason, then uncheck Server > Marked Away. Confirm the status bar
   and enabled menu actions reflect the server's replies.
4. **Join/leave/close:** Join using Ctrl+J. Leave using Window > Channel > Leave Channel, rejoin,
   then close the channel with Ctrl+W. Close a private conversation too.
   Check the adjacent conversation becomes active with its own draft and users.
   Leave several channels using the menu and confirm no empty tabs remain.
   Typed `/part` keeps the engine's reusable session as `(#channel)`, with its
   history; disconnected channel sessions also retain their names and history.
   Closing the last conversation uses the engine's normal application-exit behavior.
5. **Topics:** In a channel where you have permission, edit the topic, set a
   topic beginning with `#`, and clear it by accepting empty text. Changes
   should appear after the server confirms them. Permission errors should
   appear in chat. The topic action should be disabled outside a joined channel.
6. **Conversation context menu:** Right-click a channel for topic, leave and
   close actions. Right-click a private conversation; channel-only actions
   should be disabled. Actions should affect the clicked conversation.
7. **Search:** Ctrl+F, type text from chat, and press Enter or Next. Check
   Previous, wraparound, Match case, Unicode text, and a query with no match.
   Switch conversations and search again. Close or Escape returns to input.
8. **Copy/export/clear:** Use Window to select/copy chat. Use Window > Save Text
   and open the resulting text file; formatting should become plain text and
   Unicode should survive. Canceling a save should do nothing. Clear Chat
   with Ctrl+L and confirm another conversation's text remains.
9. **Conversation navigation:** Use Ctrl+PageUp/PageDown and Alt+1 through
   Alt+9. Order should match the visible conversation list, and drafts/cursor
   positions should follow their conversations.
10. **Unread activity:** Leave a channel in the background while a message
    arrives. Its row should become bold/colored; Alt+backtick should visit it and
    clear the marker. If possible, test a mention followed by an ordinary
    message; the mention's stronger marker should remain until you visit.
11. **Reading position:** Scroll up while messages arrive; your position should
    remain. At the bottom, new messages should be followed. Disable Follow New
    Messages and check it stops following. Ctrl+End jumps to the latest line.
12. **Panels and zoom:** Toggle the conversation list, user list and topic from
    View. Restore them using the same menu. Increase/decrease chat font size;
    check existing conversations and a newly joined channel use the new font.
13. **Display preferences:** Change font, size, wrapping and timestamps.
    Timestamp changes affect new messages, including live messages without
    server-provided timestamps. Cancel should discard unaccepted
    changes. Reduce scrollback to 100 and generate enough text to check the
    oldest lines are dropped; this intentionally discards retained text.
14. **Persistence:** Resize the window and panels, hide a panel, change display
    preferences, quit and restart. Check these settings return. Try `/quit`
    as well as closing the window. An alternate `--cfgdir` should have its own
    Qt preferences in that directory's `qt-frontend.ini`.
15. **Dialogs during changes:** Leave a topic editor open while its channel is
    closed/disconnected, or a context menu open while its conversation closes.
    Accepting/dismissing it should not act on a different conversation or crash.

## Shortcuts

| Shortcut | Action |
| --- | --- |
| Ctrl+S (also Ctrl+Shift+N) | Network List |
| Ctrl+J | Join Channel |
| Ctrl+W | Close current conversation |
| Ctrl+Q | Quit |
| Ctrl+T | New server tab |
| Ctrl+M / Ctrl+Shift+M | Reset / move to marker line |
| Ctrl+Shift+C | Copy chat selection |
| Ctrl+F7 | Toggle User List |
| Ctrl+PageUp / Ctrl+PageDown | Previous / next conversation |
| Alt+1 … Alt+9 | Conversation by position |
| Alt+A | Toggle Marked Away |
| Alt+backtick | Next unread conversation |
| Ctrl+F | Find in current chat |
| Ctrl+G / Ctrl+Shift+G (also F3 / Shift+F3) | Next / previous search match |
| Ctrl+L | Clear current chat |
| Ctrl+End | Jump to latest message |
| Alt+I | Focus message input |
| Standard Zoom In / Out (usually Ctrl++ / Ctrl+-) | Chat font size |
| Escape in search | Close search and focus input |

## Automated checks

```sh
meson test -C build-qt --print-errorlogs
```

The frontend integration test runs the real window with an in-memory engine
adapter and an isolated temporary configuration directory. It covers command
targets, action enablement, topics, Unicode cursor movement, per-conversation
drafts, unread navigation, search, export while switching tabs, display settings,
scrollback limits, restart persistence and disappearing/reused session pointers
while dialogs or context menus are open. It does not verify server permissions,
network reconnection, or actual IRC protocol traffic.

Spell checking batch: enable Spell checking under Input Box and choose an installed
dictionary tag. Check misspellings and right-click corrections in main and detached
inputs, Ignore Word across conversations, and Add to Dictionary after restart.
Verify commands, nicknames and URLs are skipped, and check long scrolled drafts,
right-to-left input and IME composition on your desktop. Automated tests cover
dictionary loading, personal persistence, exclusions, rendered underlines, and
stale context menus after switching conversations.

Native styling batch: toggle input font/colors and nickname visibility under
Input Box, and user-list font/colors under Interface. Check existing main and
detached windows, a newly detached window, Cancel, and restart. Turning styling
off should restore theme fonts/colors; user lists should never tile the chat
background image. Native zoitechat.conf settings are authoritative.

Nickname box batch: click your nickname in main and detached windows, accept
and cancel changes, and switch conversations while a prompt is open. Check your
rank icon through voice/operator changes and channel switches; toggle Show user
mode icon in nickname box. Mark yourself away/back and verify the nickname color.

In the same nickname/user-list batch, toggle graphical mode icons and hostname
display under Interface. Verify selections and moderation targets remain correct
when hostnames are visible, including detached windows and filtered lists.

User-list behavior batch: try all five native sorting modes with differently
ranked users. In Unsorted, new arrivals appear first without later metadata or
prefix updates moving existing entries. Try a custom activation command such as
`WHOIS %s`, and an empty command, in main and detached lists. Check the privilege
sort menu shortcut, restart persistence, and explicit popup Query/WHOIS actions.

Counts/meters batch: toggle Show user count in channels, and try all four lag
and throttle meter modes under Interface. Check main and detached windows from
different servers, queue flush, disconnect/reconnect, and tooltip values. In a
detached window, filter known nickname/host/account metadata, clear the filter,
and confirm counts and selections follow that window’s conversation.

Switcher batch: try Never, Always, and Only requested conversation focus policies
with explicit joins/queries and unsolicited private messages. Test Unicode label
shortening and full-name tooltips, wheel navigation on/off, and middle-click close
on a background conversation. Check the private-message auto-open, extra server
conversation, and notice-placement controls against real IRC traffic.

Server-family tree batch: connect to two servers and open several channels and
queries on each. Toggle alphabetical sorting, rename a query or channel, collapse
a family, then navigate with keyboard/wheel and numbered shortcuts. Move siblings
and entire families with bindings. Close a family root while siblings survive;
verify their histories, drafts, unread markers and actions remain independent.

Copy/title batch: test native marker and title options in main/detached windows.
Select colored multi-line text and try the timestamp/color copy controls under
Advanced, mouse release, keyboard Copy and popup Copy. Hold Shift/Ctrl while
selecting to include timestamps/IRC attributes. Paste into a plain editor to
inspect copied text or another IRC draft to inspect controls. Test message and
restored-scrollback stripping with native Colored Nicknames and per-channel
Strip Colors overrides. Topic stripping and focused rendering now have dedicated checks below.

Startup batch: toggle Skip Network List and restart with/without saved
autoconnect networks and IRC URLs. The Network List should open once without
blocking connections or the main window; close/reopen and connect from it. Try
`--minimize=0`, `1`, and `2` on a real desktop, including disabled/unavailable
tray support, and verify the application remains reachable. The runtime DCC
fixture explicitly sets skip-list/server-conversation preferences for isolation.

Native tray batch: toggle system-tray enable, close-to-tray, minimize-to-tray,
and unread blinking under Alerts. Check menu synchronization and restart. With
tray support present, minimize and close should hide the main window; without
it, the application must remain reachable. Toggle blinking during unread
activity and check a steady unread icon, later read reset, and plugin animations.

Tray away/back batch: enable Automatically mark all servers away/back and use
two connected servers. Hide/restore through close, minimize, tray activation and
GUI commands; verify server AWAY replies and status on both. Disable the option
while hidden and verify restore respects the new setting. Check the manual tray
Away/Back actions with all-back, all-away and mixed servers. No-tray fallback
should minimize normally without marking away. Real hide/restore requires a
desktop tray; offscreen tests cover policy, routing and unavailable-tray fallback.

View menu batch: check supported native control ordering and Network Meters >
Off/Graph/Text/Both in main and detached windows. A choice should update both
meters and the other window’s checked action. Set different lag/throttle modes
in Preferences and verify the submenu does not claim a common mode. Check lag
updates after enabling the meter and restart persistence of menu selections.

Tabs/Tree batch: switch layouts from View and Preferences, verify native restart
persistence, and navigate with clicks, keyboard, wheel and numbered shortcuts.
Check separate drafts, unread colors clearing on read, long/Unicode/ampersand
labels, tab scrolling with many channels, popup actions, background middle-click
close, detached conversation entries, and hiding/showing the whole switcher.
Native alternate placement and drag behavior remain separate gates.

Pane placement batch: try all switcher positions in Tabs and Tree modes, all
user-list upper/lower positions, both panes sharing a side, conflicting slots
and Hidden. Vertical tab text should remain horizontal. Resize dock splitters
and restart; confirm placement and sizes, drafts while moving, and left/right
user lists in detached windows. Pane drag placement is still pending.

Switcher appearance batch: toggle original icons and dotted lines, and try all
three text sizes in Tabs and Tree. Check unjoined channels, unread/read styling,
chat/user-list font-color styling, dark/light themes, RTL layout and expansion
with lines disabled. Automated checks compare original icon pixels, scale factors
and rendered branch pixels; desktop font/theme behavior still needs checking.


## Native completion and input options

1. Set the completion suffix to `:`, a Unicode character such as `é`, and empty.
   Check completion at the start of a line in main and detached inputs; expect
   the suffix plus a space. Inside a sentence it should not append the suffix.
2. Cancel, save and restart. `completion_suffix` in zoitechat.conf is authoritative;
   old Qt-only suffixes are superseded. Try `/set completion_suffix !` and
   `/GUI APPLY`; both input types should update. Multiple characters or UTF-8
   values above the native three-byte limit must be rejected transactionally.
3. Toggle automatic completion on send. With suffix `,` and Arthur in the
   channel, `Ar, hello` should expand only when enabled. Enable percent expansion
   and try `%Bbold%O`; enable numeric codes too and try `%065` (A). Disable
   expansion to send percent text unchanged. Save/restart all three options.

## Topic formatting and stripping

1. Receive IRC-colored/bold/italic topics with literal `<b>` text and surrounding
   spaces. Unfocused bars should render attributes; focus/select should expose
   original protocol text. Try Unicode/RTL, long topics, selection and scrolling.
2. Submit changed and empty topics. Display should wait for confirmation and
   preserve controls/spaces. Toggle topic stripping, Cancel/save, and restart.
   Message stripping must remain independent. Disabling topic stripping should
   recover retained attributes in main and detached windows.
3. Change `/set text_stripcolor_topic` then `/GUI APPLY`. Check both views,
   immediate detached updates and unchanged selections during timer refresh.
   Focused editing now renders attributes too; the raw controls remain in the
   text and clipboard. Exercise pointer selection and caret movement in both views.

## Follow channel text

1. Enable Follow New Messages and jump to the bottom. Receive joins, quits,
   modes, topics and long/multiline status text. All transcript output should
   follow. Switch away, receive background output, then return: remain at bottom.
2. Repeat while intentionally reading old text; preserve that reading position.
   Resize/narrow the window, move panes and detach/attach while following wrapped
   output. Repeat in detached windows. Disable following and verify preservation;
   Jump to Latest should still work independently.

## IRC protocol runtime regression

Run `meson test -C build-qt-runtime qt-irc-runtime --print-errorlogs` in a native
Linux plugin-enabled Qt build. It uses the actual Qt executable/C engine and
isolated temporary configuration/addons against a loopback server. Localhost
sockets must be permitted. The probe uses UI bridge entry points; the peer
independently checks registration/PING/PONG, NAMES/operator state, live
JOIN/NICK/PART/QUIT membership, rejected/confirmed modes, topic controls/spaces,
bidirectional messages, PART, reconnect/rejoin and disconnect cleanup.

TLS-enabled builds generate temporary certificates with the `openssl` CLI and
exercise the saved-network bridge with SASL PLAIN: trusted certificates, explicit
self-signed overrides, untrusted certificates and hostname mismatches. Rejection
must send no IRC credentials. A denied SASL login checks abort/CAP completion and
the engine's existing continued-registration behavior. Trust files are scoped to
test processes; system trust is unchanged. SASL EXTERNAL requires a verified client
certificate loaded from the native network-specific `certs/<network>.pem` path;
the peer checks the empty authentication identity, no password disclosure and
certificate presentation again after reconnect. Non-TLS builds explicitly skip TLS.

Recovery scenarios drop an established connection, refuse the initial connection
until failure is observed, and cancel pending reconnect. Checks cover waiting
state, cleared membership/disabled moderation, automatic rejoin into the original
conversation with its draft retained, and no new socket after cancellation.
SCRAM SHA-1/SHA-256/SHA-512 scenarios independently verify client proofs using
Python's hashlib/hmac, complete server verification, and repeat after reconnect.
Invalid server signatures and malformed verifier prefixes must abort SASL before
CAP completion. These tests reproduced and guard the core verifier-prefix fix.
Real desktop interactions and external-server behavior remain separate gates. Network Preferences exposes `irc_reconnect_rejoin`: save/restart
and drop a connection with joined channels, testing both enabled and disabled.

## Pane dragging

1. Ctrl+Shift-drag the main tree, tab bar and user list. Test the movement
   threshold, preview/highlight, all four side slots, Escape and outside drops.
2. Drop onto an occupied slot: the other pane should move to the adjoining slot.
   Check both settings after restart, including drafts, selections and unread
   state. Tabs can drop at top/bottom center; Tree/user lists remain on sides.
3. Test normal clicks, Ctrl/Shift user selection, popup/middle-close, metadata
   tooltips, multiline text and file drops. Foreign-window drags must not move
   panes. A failed config save must show an error and retain the old placement.
   Automated drop tests cover positions, atomic collisions/save failure and
   draft/document identity; real gestures, previews and Escape remain desktop checks.

## Keyboard activity navigation

1. Background attached conversations and receive private highlights/messages,
   channel highlights/messages and status data. Alt+backtick should use the
   original five priority classes and newest-first order within each class.
   Reading clears styling, including an already displayed background conversation.
2. Detached conversations must be skipped without consuming their queue entries.
   Detached Change Page/auto must leave the main window alone. Check normal
   navigation, valid numbered/relative bindings and explicit window activation.
3. Absolute Change Page zero or past the last tab must leave selection unchanged.
   Automated tests cover delegation/filtering, native order, detached isolation,
   read styling, empty queues and invalid pages; desktop key mapping remains open.

## Plugin key events

1. Load a plugin/script hooking Key Press. Log keysym, modifier mask, UTF-8 text
   and byte length. Test letters, Ctrl/Shift, Unicode, F-keys, arrows, reverse Tab,
   keypad/dead accents, Caps/Num Lock, AltGr and non-Latin layouts in main and
   detached inputs, on X11/Wayland and other supported platforms.
2. Consume a bound key and an ordinary typing key with EAT_ZOITECHAT or EAT_ALL.
   Neither binding nor editing should run. EAT_NONE should preserve normal
   behavior; EAT_PLUGIN should only stop later hooks. Held keys still reach the
   hook before the existing command-repeat policy.
3. Close, replace or switch the source in an EAT_NONE hook. The original key
   must not act on a different conversation. Repeat during detached close/attach
   and binding reload. Check menu accelerators, multimedia/platform shortcuts and
   IME composition separately: only key presses delivered to input enter this path.
   Automated tests cover bridge arguments/consume, translation, hook ordering,
   replacement and main/detached context. Real plugin lifecycle/platform checks
   remain desktop/runtime gates.


## Custom menu icons

1. Add runtime `/menu -i` entries with absolute and config-relative image paths,
   then theme names and common GTK stock names such as `gtk-open`. Check main
   and detached menus and all popup roots. Files take precedence over names.
2. In Usermenu or saved popup editors, use `Label~menu icon.png~` with an image
   in the active config directory. The visible label should omit the icon suffix;
   the file and command text must preserve their original data. Escaped tildes
   should stay literal. Test an alternate `--cfgdir` and restart saved entries.
3. Update, remove and invalidate an icon. Menus should refresh without running
   commands, and missing icons should leave commands enabled. Check light/dark
   themes, image scaling, high-DPI screens, shortcuts, toggles and submenu icons.
   Runtime entries still are not saved automatically.

Automated tests cover C bridge icon paths, config-relative resolution, actual
rendered pixels in main/all popup roots, stock fallback, missing/update/clear,
saved labels/escaping and preserved command execution. Platform menu rendering
and theme availability remain desktop checks; Runtime Pango labels now have separate checks below.


## Runtime Pango menu labels

1. Add `-m` runtime main, submenu and popup labels using nested bold/italic,
   foreground/background colors, relative/absolute sizes, entities and underscores.
   Check top-level and detached menus as well as all popup roots. Plain submenu
   paths must still resolve, including children added before their parent.
2. Test selection/disabled states, checkboxes/radios, icons, shortcuts, submenu
   arrows, Alt mnemonics and escaped `__` underscores. Invalid markup should stay
   literal; refreshing must not execute commands. Delete/recreate an identity and
   verify an unmarked label does not inherit old styling.
3. Try high DPI, RTL text, large fonts, multiline labels and light/dark desktop
   themes. Check style spacing/clipping and keyboard selection. Styled menu bars
   use Qt widget rendering rather than native platform menu export.
4. Build/package with Pango/Cairo development libraries (`libpango1.0-dev` on
   Debian/Ubuntu, `pango-devel` on Fedora, `pango` on Arch). Both AppImage container
   recipes include this dependency; a new packaged build still needs validation.

Automated tests use the native parser and check cache lifetime/stripped core
labels, entities, malformed input, high-DPI dimensions, size/foreground pixels,
real menu-bar/menu/all-popup rendering, mnemonics, paths and command routing.
Platform style spacing, menu export and packaged runtime checks remain open.


## Core prompt lifecycle

Open string, integer, boolean and file requests, then destroy their dialog or
parent. Expect one cancellation callback (last text/integer value, false for a
boolean, or the file completion sentinel). Accept/cancel normally and ensure
later destruction does not duplicate completion. Completion is queued until Qt
has unwound the response/teardown handler. Test a multi-file callback that closes
its parent after the first path: all accepted paths and one sentinel must arrive.
Automated tests cover these lifecycle paths, including reentrant parent deletion.


## Native formatted input editing

1. Toggle Input Box > Render colors and attributes in input, Cancel/save and
   restart. It uses `gui_input_attr` in zoitechat.conf; `/GUI APPLY` should update
   main and detached inputs. Check fonts/colors with input styling both on/off.
2. Insert bold, italic, underline, reverse, reset, numeric and RGB colors. Try
   partial color codes and pasted formatting. Raw controls must survive copy,
   undo/redo, completion, history and sending while the input displays attributes.
3. Click, Shift-click, double-click and drag words; extend selection past both
   edges and hold there to autoscroll. Check caret visibility in long drafts,
   trailing spaces, RTL text, emoji and reverse/background colors. Test context
   menu spelling corrections, raw selected copy, single/multiline text drops and
   X11 primary-selection paste. Selections must use displayed glyph positions.
4. Compose with a real IME. Preedit temporarily uses native unformatted editing;
   commit/cancel should restore formatting without losing controls or draft text.
   Candidate windows should track the caret. Try keyboard layout changes, touch
   selection and accessibility on supported desktops.
5. Repeat focused editing/selection in main and detached topic fields, including
   read-only/disconnected topics and native topic stripping. Sending must use the
   original text and wait for server confirmation.

Automated tests cover raw-position preservation, focused RGB pixels, pointer/word
selection, copy/undo, long/trailing-space scrolling, RTL/emoji caret bounds,
synthetic IME preedit/commit, option toggling and main/detached native preference
updates. Real IME, touch, accessibility and platform clipboard behavior remain
explicit desktop checks.

## Optional native Usermenu

The top-level Usermenu is hidden by default, matching ZoiteChat's `gui_usermenu`.
Enable General > Show Usermenu or `/set gui_usermenu 1` followed by `/GUI APPLY`;
main and detached windows should show it in the native position between Server
and Settings. Disable it and repeat after restart and detaching a conversation.
Hiding the menu must preserve its configured commands. Automated frontend tests
cover the hidden default, new detached windows and live enable/disable in both;
the native preference bridge test covers the configuration field.

## Native View visibility settings

Menu Bar, Topic Bar, User List, User List Buttons and Mode Buttons now read/write
`gui_hide_menu`, `gui_topicbar`, `gui_ulist_hide`, `gui_ulist_buttons` and
`gui_mode_buttons` in zoitechat.conf. They use native defaults and imported settings;
old Qt-only `view/menu`, `view/topic`, `view/users`, `view/userButtons` and
`view/channelModes` keys no longer override them. Follow New Messages, channel
switcher visibility and Dialog Buttons remain Qt display settings.

Toggle each View item from main and detached windows; both should update and
retain the setting after restart. Ctrl+F9 must restore a hidden menu. Change the
native values with `/set`, then `/GUI APPLY`, and save/cancel the Interface page
in Preferences. Failed saves must restore the previous checks and visibility.
Automated tests cover both-window menu changes, native field writes, failed-save
rollback and recreation of the frontend with the saved native values.

## Staged installation and desktop registration

A Qt-only full native build was installed into
`/tmp/zoitechat-qt-install-audit` using Meson's DESTDIR support. The audit checked
its Qt desktop launcher and safe-mode command, D-Bus activation executable,
installed PNG/SVG icons, matching AppStream desktop identity, all six enabled
plugin shared-library dependencies, and startup of the staged executable with
an isolated configuration and the offscreen platform. The GUI startup check
used `--no-auto --no-plugins`; linked dependencies alone do not prove script
plugin execution. Desktop-file and offline AppStream validation are now Meson
tests when the corresponding validators are available. The frontend regression
also checks desktop identity and its embedded application icon fallback.

Repeat from a clean staging directory after packaging changes. On a real desktop,
launch from the application menu, test safe mode, `irc:` and `ircs:` URL handling,
window grouping/icon, and D-Bus activation in Qt-only and combined GTK/Qt installs.
The AppImage packager now selects the launcher belonging to `APP_BIN` rather than
whichever desktop file happens to be first. A rebuilt AppImage still needs launch
and dependency checks on the target distribution; the existing user-owned file
in `out/` was not replaced by this audit.

## Real scripting plugin lifecycle

Run `meson test -C build qt-scripts-runtime --print-errorlogs` in a native Linux
Qt build with plugins enabled. It copies the configured Python/Perl/Lua shared
modules and the three installed Python helper modules into an isolated relocated
layout. Its C probe drives the actual addon bridge to load scripts from paths
containing spaces, reload/unload them, and inspect real addon rows. Scripts verify
Unicode input and command/Key Press callbacks; callbacks must stop after unload
and must not duplicate after reload. Python's old unload callback must finish
before the replacement script starts. The probe also reloads the native Python
interface while a script is active, checks removal of the old script/hooks, and
loads and exercises a new script through the replacement interface. This exposed
and now guards runtime helper-path lookup and deterministic Python cleanup fixes.

The test needs no IRC server or localhost socket permission. It selects configured
languages from Meson's build options and skips when all are disabled. It isolates
configuration and addon paths, removes inherited Python path/home overrides, and
disables Python user-site imports. Desktop key gestures, plugin-provided menus
and prompts, and third-party plugins that depend on GTK widgets remain separate
compatibility checks.

## Legacy nickname-cycling key action

`Change Selected Nick` is a legacy alias for input nickname completion in the
original `fe-gtk/fkeys.c`; it does not move the user-list selection. Qt now uses
the same completion path as `Complete nick/command`, cycling forward with empty
Data 1 and backward when Data 1 is set. The frontend regression sends actual
Shift+Page Down/Up key events through imported bindings, checks both cycling
directions, and checks that user-list selection is preserved. Repeat with a
custom key assignment and in a detached input on a real desktop.

## Removing and remapping input shortcuts

The main and detached inputs now delegate IRC formatting, history, completion,
and replacement triggers to the configured binding table. They no longer run a
second hardcoded copy of these shortcuts after the table declines a key. A
missing keybindings.conf still loads the native 39-row default table; an existing
custom file replaces that table. Standard platform editing/paste keys continue
to work through Qt when unbound.

The frontend regression uses a custom file with the default Ctrl+B, Up, Tab and
space replacement bindings removed. Those keys must not format, recall history,
complete or replace words. Remapped formatting, history and command-completion
keys must still work, including the configurable command prefix. Standalone
ChatInput tests retain their own default shortcut behavior. On a desktop, repeat
removal/remapping from the keybinding editor, use `/GUI APPLY` after external
changes, and check both main and detached inputs after restart.

## AppImage trust store portability

The Ubuntu 22.04/Qt 6.2 AppImage was rebuilt and its extracted AppRun passed all
16 IRC/TLS scenarios on an isolated X11 display. Its bundled OpenSSL names
`/usr/lib/ssl/cert.pem` and `/usr/lib/ssl/certs`, which are absent on this host.
`ssl-trust` reproduces failure with those default paths, then verifies a current
host trust anchor through the portable fallback, rejects an unrelated generated
certificate, and checks empty/custom explicit trust overrides. This uses the
actual extracted AppImage libraries, not the host OpenSSL libraries:

```
python3 src/common/ssl-trust-test.py build/src/common/ssl-trust-test /path/to/squashfs-root/usr/lib --require-fallback
```

Fallback discovery runs only when both compiled default locations are absent
and neither SSL_CERT_FILE nor SSL_CERT_DIR is explicitly set. It loads the host's
CA bundle rather than shipping a frozen copy of trusted roots. Certificate and
hostname verification remain enabled. The IRC driver accepts an optional build
configuration directory for packaged executable tests; `QT_IRC_PLATFORM=xcb`
selects X11 instead of its default offscreen test platform.

## Dark chat contrast

Timestamp text now targets 7:1 contrast against its chat background, and
theme-link text targets at least 4.5:1. Numeric IRC foreground colors and nickname/activity
colors are lifted when necessary on a dark default chat background. Configured
palette values stay stored as chosen; the adjustment is a display treatment.
Explicit IRC foreground/background pairs and explicit RGB text retain their
specified values. Copying formatted chat preserves the original color values.

Formatting regressions use two dark backgrounds, all 32 numeric IRC slots,
actual timestamp/link document fragments with deliberately invisible theme colors,
explicit foreground/background and RGB examples, and formatted-copy round trips.
On the desktop, check timestamps, joins/parts, links and colored nicknames in main
and detached windows with the default dark theme. Background images, arbitrary
explicit color pairs and very small fonts still require visual review.

The combined certificate/contrast test candidate was rebuilt on Ubuntu 22.04 with
Qt 6.2.4. All 43 available non-runtime tests passed in that container, alongside
45 tests in the full native build. The final extracted AppRun then passed all
16 IRC/TLS scenarios on X11, and its bundled crypto libraries passed the host,
empty and custom trust-store regressions. The Qt 6.2 run also exposed and guards
a menu-icon fix: theme icons must produce a pixmap before they are accepted as
available. The candidate is `out/ZoiteChat-ng-Qt6-test.AppImage`; the existing
`out/ZoiteChat-ng-x86_64.AppImage` was preserved.

## AppImage bundled addon discovery

The packaged executable now resolves its configured addon directory under
APPDIR when running from an AppImage, while an explicit nonempty ZOITECHAT_LIBDIR
keeps precedence. The packager includes the matching OpenSSL 3 providers and a
portable AppRun launcher that sets OPENSSL_MODULES when it is unset. This fixes
Checksum discovery and FiSH initialization on a host whose filesystem and crypto
version differ from the build container.

`appimage-addons-test.py` runs the real AppImage with an isolated config, unsets
library/provider overrides, and checks that exactly one Checksum and FiSH addon
is loaded from the mounted image. The previous candidate reproduced zero bundled
addons; the path-only fix loaded Checksum but exposed the missing FiSH provider.
The complete fix loads both. Its process-group cleanup includes the AppImage
runtime and its child executable. Run it under X11 or `xvfb-run -a`.

The IRC runtime driver's `--bundled-addons` flag leaves default addon discovery
enabled, so a packaged regression run also exercises normal plugin startup.
The default isolated native run continues to use an empty system addon directory.

## Detached server action state (post-RC1 follow-up)

Detached Server/Window actions now follow their own conversation's connection
state. Away is checkable and reflects confirmed engine state while an away/back
request is pending. Disconnect remains available during reconnect wait so it can
cancel the retry. Join, channel list, ban/settings tools and topic editing are
disabled as appropriate after disconnect, then recover on reconnect. Other
conversations' checks and availability must remain independent.

The frontend regression holds away acknowledgements pending, checks command
routing to the detached source, confirms the new state, disconnects it while the
main conversation remains connected/away, tests reconnect cancellation availability,
and restores the connection. This follow-up is intended for the next candidate;
the published RC1 tag remains the reference for its existing assets.
