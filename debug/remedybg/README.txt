RemedyBG is a user mode 64-bit Windows debugger written from scratch.

Contact Information
-------------------
George Menhorn
remedybg@protonmail.com
https://remedybg.handmade.network/
https://remedybg.itch.io/remedybg
https://github.com/x13pixels/remedybg-issues/issues
Twitter: @x13pixels

I welcome any and all bug reports, feature requests, praise, and/or curses. Please feel free to
use the Github issue tracker, listed above, to report any problems.

Thanks to Visual Studio for providing the motivation, in the form of aggravation and want, for
making we want to write a debugger from scratch in the first place.

Thanks to Casey Muratori for his inspiration, suggestions, and the rekindling of the handmade
movement!

Thanks to Simon Anciaux for the extraordinarily detailed feedback on pretty much every release that
I've put out to date.

Usage
-=-=-
 remedybg.exe [exe-filename | rdbg-filename]

 Executing remedybg.exe without any command-line parameters will launch RemedyBG with an unnamed
 session. You can fill in the necessary details for launching an executable in the Session menu.

 exe-filename: Specify an executable to debug. This will auto-populate the session's "Command".

 rdbg-filename: Specify an .rdbg session file to load. This will reload the "Command", "Command
 Arguments", "Working Directory", any files that were opened, and any breakpoints that were set.

Driving an existing instance
-=-=-=-=-=-=-=-=-=-=-=-=-=-=
   An existing instance of RemedyBG can be driven via the command-line. Following are the currently
   supported commands and their arguments. Optional arguments are enclosed in brackets '[]'.

   remedybg.exe open-session session-filename
   remedybg.exe open-file filename [line-number]
   remedybg.exe close-file filename
   remedybg.exe start-debugging
   remedybg.exe stop-debugging
   remedybg.exe attach-to-process-by-id process-id
   remedybg.exe continue-execution
   remedybg.exe add-breakpoint-at-file filename line-number
   remedybg.exe remove-breakpoint-at-file filename line-number
   remedybg.exe add-breakpoint-at-function function-name
   remedybg.exe remove-breakpoint-at-function function-name

Sessions
-=-=-=-=
   A session in RemedyBG is a simple binary file that, presently, contains information about the
   command, command arguments, working directory, any source files that were opened, along with any
   breakpoints that were set.

   From the 'Session' menu you can either open a saved session (.rdbg file) or enter the command,
   command arguments (if any), and working directory (if different than the command's path). 

Storage
-=-=-=-
   Files for storing UI state, and so forth, are located in the current user's application data
   directory.  You can get to this directory in Windows explorer by typing "%APPDATA%\remedybg". In
   the case that the smoke hits the fan and you want to reset RemedyBG to its default state, you can
   delete the files in that directory (or simply delete the entire remedybg folder).

   If you prefer your settings to be stored in an alternate location, you can start RemedyBG with
   the "--settings" parameter and specify the alternate folder. For example:

      remedybg --settings C:\your\settings\folder

Watch Window
-=-=-=-=-=-=
   Registers
   ---------------
   The watch window can parse register names such as RAX, RIP, EAX, AL, and so forth. The register
   names are case insensitive (e.g., Rax or rax work as well). Note that variables in scope take
   precedence over a register name. So, for example, if you had a local variable name rsp, then the
   watch window will show that variable's value and not the register name. To override any variable
   shadowing, you can prefix the variable name with a '$' (e.g., $RAX).

   Relative Scopes
   ---------------
   A watch window expression can indicate the relative scope for which to perform a variable lookup.
   The syntax for this uses a prefix of the form "{^n}" where n >= 0. For n = 0, this is the normal
   lookup at the current scope. For n > 0, the resolution of an identifier is done 'n' frames above
   the current frame. In addition, you can use '{^*}' to search up the stack to the first frame that
   properly resolves an identifier. Some examples of this would look like:

       {^1}foo          # Resolve 'foo' in the parent frame
       {^*}(char*)&foo  # Resolve expression in first ancestor containing the identifer 'foo'

   Locking Scope
   -------------
   For an expression in the watch window you can toggle the lock icon. Locking an expression will
   maintain the scope of expression.

   Format Specifiers
   -----------------
   A format specifier can alter the formatting, or display, of the value of an expression in the
   watch window. The syntax of format specifiers is a comma at the end of an expression followed by
   the format specifier. Valid format specifiers are the following:

   -- x or X : Using the 'x' format specifier will cause the output of any values to be in hexadecimal,
   rather than decimal. Note that this format specifier takes precedence over the specifier for
   number of elements. Similarly, using 'X' will display upper-case hexadecimal values where
   appropriate.

       var, x           # Display 'var' and any children in lower-case hexadecimal
       var, X           # Display 'var' and any children in upper-case hexadecimal

   -- <num elements>: This is for specifying the number of elements to display in an array or pointer
   type. The value can be an integer literal or an integer/enum value that can be resolved in the
   current scope. The <num elements> expression can be used in conjunction with another format
   specifier.

       arr, 16            # Display 16 values of the expression 'arr'. 
       arr, count         # Display 'count' values of the expression 'arr'. 
       arr, X count       # Display 'count' values of the expression 'arr' in upper-case hexadecimal

   -- str : Using the 'str' format specifier will show an inline, multiline string view in the watch
   window. This format specifier can be followed with a value to indicate the number of characters
   to display. The value can be an integer literal or an integer/enum value that can be resolved in
   the current scope.

      ShaderText, str             # Displays the contents of 'ShaderText' in an inline string view
      text.Data, str text.Count   # Displays 'text.Count' characters of 'text.Data' in an inline string view

   -- _ : A no-op format specifier that can be used to disambiguate between a format specifier and a
   local variable use as a <num elements> expression.

      arr, x              # Display array 'arr' in lower-case hexadecimal
      arr, _ x            # Display 'x' values of the expression 'arr'
      arr, x x            # Display 'x' values of the expression 'arr' in lower-case hexadecimal

   Synthetic Structures
   --------------------
   Synthetic structures are identifiers that can be used in the watch window that aren't actual
   types in the target being debugged per se but are, instead, synthetized from current state of the
   target. Valid synthetic structures are as follows:

   -- __locals : Displays the list of local variables starting from the innermost scope out to
   function scope.  Note that __locals can be combined with a relative scope operator to view local
   variables in parent scopes.

      __locals       # Displays local variables in the current scope
      {^1}__locals   # Display local variables in the parent scope

   -- __params : Displays the list of function parameters. Note that __params can be combined with
   a relative scope operator to view function parameters in parent scopes.

      __params       # Displays the current function's parameters
      {^1}__params   # Displays the parent function's parameters


Keyboard Shortcuts
-=-=-=-=--=--=-=-=

   Ctrl+Tab             Switch active window
   Ctrl+Alt+[0-9]       Switch to a saved layout
   Ctrl+B               Add function breakpoint
   Ctrl+G               In text window, brings up the Go To Line popup
   Ctrl+O               Open source file
   Ctrl+Shift+O         Open session
   Ctrl+S               Save session
   Ctrl+Shift+X         Break execution
   F5                   Start debugging
   Shift+F5             Stop debugging
   Ctrl+Shift+F5        Restart
   F6                   Attach to process
   Shift+F6             Detach
   F9                   Toggle breakpoint in text window
   Ctrl+F9              Toggle breakpoint enabled in text window
   F10                  Step Over (Line); when disassembly window is active, this behaves as Step Over (Instruction)
   Ctrl+F10             Run to Cursor (can be used even when the target has not been launched)
   Ctrl+Shift+F10       Step Over (Instruction)
   F11                  Step Into (Line); when disassembly window is active, this behaves as Step In (Instruction)
   Ctrl+Shift+F11       Step Into (Instruction)


Attribution
-=-=-=-=-=-

Although, the debugger engine is written completely from scratch, the RemedyBG application as it
stands today would not be possible without the following awesome tools:

 * Intel XED: https://github.com/intelxed
 * Dear ImGui: https://github.com/ocornut/imgui
 * Omar's mini memory editor for ImGui: https://github.com/ocornut/imgui_club/tree/master/imgui_memory_editor
 * Some bits from ImGuiColorTextEdit from https://github.com/BalazsJako/ImGuiColorTextEdit

Changelog
-=-=-=--=

Version 0.2.9.0
---------------
 * Fixed Go To in the case that given line number is negative.
 * Using the Del key while editing a watch expression will no longer delete a watch expression
 inadvertently.
 * Fixed crash when expression is an enum that is forward declared in a PDB.
 * Updated acceleration structure for line number information to handle additional cases found in
 PDBs.
 * Protect disassembly output when reading inaccessible pages of memory.
 * Reset working directory upon new session.
 * Double-click now selects word in text editor.
 * Step in when IP is on a CALL will now skip a subsequent JMP instruction.
 * Fixed hexadecimal format specifier when used in character arrays.
 * Added support for the lookup of static local variables.

Version 0.2.8.9
---------------
 * Fix to allow line number information in the symbol file to be in an arbitrary order. This fixes
 the problem reported for PDBs output from the Zig compiler.
 * Added __locals synthetic structure. This identifier can be used to display the local variables in
 the current (or specified) frame.
 * Added __params synthetic structure. This identifier can be used to display a function's parameters
 in the current (or specified) frame.
 * Added a "Send to Memory Window" option in the watch window to allow quickly copying an expression
 from the watch window to the memory window.
 * Fixed a watch window crash that happened when a watch expression is deleted while in the middle
 of the watch expression render loop.
 * Auto-scroll the output window only if the scroll position is at the end.
 * Added 'attach-to-process-by-id' to command-line driver commands.
 * The string view now spans the entire width of the watch window.
 * Fixed parser for format specifiers to allow both hexadecimal format specifiers along with an
 expression to control number of array elements, e.g., "some_array, x 5".

Version 0.2.8.8
---------------
 * Fixed bug in client-side traversal of callstack argument type and values that was introduced
 after switching to a common debug protocol.

Version 0.2.8.7
---------------
 * Some improvements to recently added string view based on user feedback:
   * String view no longer captures mouse scroll events, passing scroll events to the parent window.
   * Fixed computation of string view height to take into account the horizontal scroll bar.
   * Now allow specification of string length in "str" format specifier.

Version 0.2.8.6
---------------
 * Allow both 32-bit and 64-bit floating point literals in a watch expression.
 * Corrected display of boolean expressions in the watch window.
 * Added handling of enumerations with a specified integral type.
 * Memory window settings are now persisted to disk.
 * Fixed one off bug in traversal of symbol information when handling structures/classes that contained nested types
 * Fixed member variable lookups for base classes. This fixes a reported problem when trying to
 inspect a std::string.
 * Added format specifier, "str", for displaying an inline string view.

Version 0.2.8.5
---------------
 * Fixed a timing bug in the UI that could result in a failure to display an evaluated expression's
 data.
 * More robust handling of stack unwinding failures.

Version 0.2.8.4
---------------
 * Fixed crash when attempting to resolve an identifier after unsuccessfully unwinding the stack
 (e.g., upon jump to zero)

Version 0.2.8.3
---------------
 * Added -FLT_MIN, -FLT_MAX, -DBL_MIN, and -DBL_MAX as known constants as FLT_MIN != -FLT_MAX.
 * Toggling a window via the menu will now bring it in focus
 * We now display an error message in the output window and in the settings menu in the case that a
 font cannot be loaded.
 * Go to line will now scroll even if the line number is already the current line.
 * Fixed a glitch where initial scroll to a line would fail to reach target line
 * Watch expressions are now stored in the RDBG file
 * Added support for adding breakpoints to one or more overloaded functions

Version 0.2.8.2
---------------
 * Fixed a regression: removed ImGui's assertions from a release configuration. This fixes a dialog
 box that pops up if the configured font cannot be found on disk.

Version 0.2.8.1
---------------
 * Fixed problem where memory window would always become the topmost tab.
 * Added better handling of watching local variables for Clang generated PDBs 
 * Add handling for case where a forward declaration of a struct/union in the PDB file does not
 have a separate, unique name (Clang)
 * Better handling of PDBs generated from cl /ZI
 * Fixed a glitch where dragging the mouse over the application menu would inadvertently select text
 in a text/disassembly view.
 * Can now select columns in the callstack window
 * Added "Last Known Caller" column to the threads window. This shows the function name of the last
 known function in the call stack.
 * We now preserve format flags for a locked variable in watch window.
 * Disassembly will now auto-scroll to center when approaching bottom
 * Added smooth scrolling in text window when scrolling to center
 * Added "Go To Line", initiated with Ctrl+G, for text windows.
 * Fixed a regression with handling of EXE+arguments when launching RemedyBG from the command line

Version 0.2.8.0
---------------
 * Fixed an edge case where strings in the global string table of a PDB could be multiply defined.
 This can happen when building with Clang and then with VS. VS appends to the string table in the
 PDB rather than rewriting / removing duplicate entries.
 * Fixed bug where F10/F11 would not break at the entry point of a target if there was an existing
 breakpoint at the entry point.
 * Added implementation of processor breakpoints. These are available in the Breakpoints window
 under the "Processor" tab. Up to four processor breakpoints can be be added -- the maximum
 number available on the x64 architecture. Processor breakpoints can be setup to to trigger when
 memory, at a given address, is written to or read from.
 * Fixed 64-bit bitfields
 * Fixed lookup of scoped variable in the case that the instruction pointer is on the last
 instruction of a block and that instruction is a non-user breakpoint.
 * If a session file fails to open from the recent file list the user is given an opportunity to
 remove it from the list.
 * Added "Clear" menu item to Session>Recent list.
 * We now display the address of an array in the watch window. 

Version 0.2.7.4
---------------
 * Fixed retrieval of data for a string preview in the case where part of the requested area was
 inaccessible (e.g., an unreadable page of memory). 

Version 0.2.7.3
---------------
 * Fixed crash in call stack update in certain layouts of PDB frame procedure
 * Eliminated the use of the global string table's hash table that is baked into the PDB file. We
 use our own hash table now, instead. This was done, in part, to address a bug reported where the
 yellow cursor wasn't showing up in the text window when using a PDB generated by Visual Studio 2019
 (the same is true of Clang).
 * Reverts restriction to string preview of only printable, ASCII characters.
 * Fixed a bug where closing an unnamed session would not prompt the user for a rdbg filename.
 * Added "Auto Save" option to the Session menu. When enabled, any changes to the session are
 automatically saved without prompting the user.
 * Fixed handling of structure fields in watch when the structure contains a virtual function table,
 nested type, static field, enumeration, or any methods.
 * Fixed threading issue when rendering output window that could cause a crash.
 * Added "Copy to Clipboard" option in context menu of Output window.
 * Added a new context menu item in the Watch window, named "Add Watch", that can be used to quickly
 add a member as a new watch item.

Version 0.2.7.2
---------------
 * Run to cursor now works on a line containing an existing breakpoint
 * Added an option to toggle code bytes in the disassembly window
 * We now show the symbolic name for known values such as FLT_MAX, UINT_MAX, SHRT_MAX, and so forth.
 * Can now show hexadecimal representation of floating point values using the ",x" or ",X" format
 specifiers.
 * We now properly handle the lookup of variables in a block containing a function call on the
 block's last line. The lookup was adjusted to account for the RIP of the previous stack frame
 pointing to the instruction following the call and not the call itself.
 * Added Session|New to clear everything for starting a new debug session.
 * We now automatically resolve and store the full path to an EXE when launching RemedyBG on the
 command-line with a relative path to an EXE.
 * Window title now shows EXE name, if available, when now RDBG is specified.
 * Fixed potential crash where PDB reports nonzero number of parameters to a function and none of
 them are relative to RSP.

Version 0.2.7.1
---------------
 * Fixes a crash when handling expression with a const/volatile modifier on a primitive type
 * Cursor is now properly displayed when editing an expression in the watch window
 * Fixed scrolling of text for expressions in watch window
 * Fixed one-off bug when generating string previews for an array
 * An arrow is now drawn, in addition to the red highlight, for a line with an exception
 * An arrow is now drawn in breakpoint window next to breakpoint that was just hit
 * Removed leading zeros from address in breakpoint, 
 * Shortened text in breakpoint window (removed ", Ln")
 * The status text can now be displayed either on the top of the UI (default) or the bottom. Option
 in Window|Style menu as well as right-click context menu to "Move to Top" or "Move to Bottom".
 * Can now select which columns to display in the threads, modules, watch, and breakpoints windows.
 * No longer show a string preview if there are any unprintable characters in the string.
 * We now force a redraw whenever the output window is updated.

Version 0.2.7.0
---------------
 * Various windows are now visible even when the debugger is not attached to a target
 * Added basic window layout management (saving / restoring layouts)
 * Added a "Copy" menu item for values in the Watch window
 * Fixed disassembly focus indicator rendering when window is floating

Version 0.2.6.5
---------------
 * Fixed a bug where the memory pane's address input was stealing focus from the options popup.
 * The rendering of the text window's current-line arrow was changed to make it easier to see when
 on the same line as a breakpoint.
 * Removed leading zeroes from display of watch window pointer values.
 * Expressions now handle primitive type UQUAD (distinct from UINT8).
 * Handle case where a non-user breakpoint is hit while stepping through the code (e.g., stepping
 over __debugbreak() ).
 * Watch window values that change are now highlighted in red.

Version 0.2.6.4
---------------
 * Text file read from disk is now properly null terminated in the case that a BOM is not detected.

Version 0.2.6.3
---------------
 * We now properly handle arbitrary first chance exceptions in the debugger (allow normal processing
 rather than swallowing the exception). For C++ exceptions generated by VS we extract the class name
 from the exception information and display that in the output window.
 * Scrolling with arrow keys in a text window now matches the behavior of a typical text editor.
 * Expression parser now supports the indexing of array/pointer types by an unsigned char value.
 * Now support casting between non-pointer types
 * The user is now prompted if they are currently debugging a target and switch to a different
 session.
 * Switching to a new session will now close any opened files.
 * Now ensure central text node is created (behavior changed in later version of Dear ImGui). Note
 that this may require a reset of the layout (Window|Reset Layout) if you experience floating text
 windows.

Version 0.2.6.2
---------------
 * Memory edit prev/next buttons now grayed out if there is no target
 * Output window now display process ID upon detaching
 * Fixed a crash caused by trying to update memory window after target had been detached
 * We now properly unset the processor's trap flag when detaching from a process while stopped on a
 breakpoint
 * We now add any existing breakpoints and watches upon attaching to a target
 * If the target was attached then "Stop Debugging" will now detach rather than terminate. Added a
 Terminate menu item to allow explicitly terminating an attached process. We indicate whether the
 process will be detached or terminated in the "Stop Debugging" menu items.
 * Fixed a bug where memory window edit control would not release focus when Ctrl+B (add function
 breakpoint) was pressed.
 * Fixed hang when analyzing disassembly in the case that the last remaining bytes of the binary are
 invalid instructions.
 * Fixed crash in callstack generation when parameters are stored relative to RBP rather than RSP.

Version 0.2.6.1
---------------
 * Updated to the latest version Dear ImGui (docking branch)
 * Fixed refresh issue when a deferred breakpoint is resolved and the RemedyBG window is not active
 * Adds support for UTF-16 LE / UTF-16 BE encoded files

Version 0.2.6.0
---------------
 * We now allow controlling an existing instance of RemedyBG from the command-line
 * Add command-line option for specifying an alternate settings directory
 * Fixed a bug in local identifer lookups in the case that symbols for user defined types were
 interleaved with the relative-to-register symbols.

Version 0.2.5.0
---------------
 * Fixed handling of doubles vs. floats in certain expressions in watch window
 * Added ability to cast to/from primitive integral/floating point types
 * Better error messages in watch window for structure/union member access
 * Added ability to attach to a running process

Version 0.2.4.2
---------------
 * Fixed formatting of XMM/YMM/ZMM registers in Registers pane (top 32 bits were being lobbed off)
 * Can now specify xmm0-xmm15, ymm0-ymm15, and zmm0-zmm15 in the watch window
 * Can now specify register names with a '$' prefix to side step any variable shadowing.
 * Fixed a crash caused by having a format specifier, e.g. "%s", in part of a string being previewed
 in the watch window.
 * Fixed crash in call stack pane in release mode in certain cases.
 * Fixed crash if an array parameter is displayed in a detailed call stack frame.
 * Fixed crash when encountering parameter of unknown type in call stack (can happen in release
 mode).
 * Stepping type (line vs. instruction) is now stateful based on the last type of window that had
 focus (text or disassembly) and not whether the disassembly window currently has focus. Clicking on
 another pane (other than a text window) will no longer reset the state back to line-based stepping.

Version 0.2.4.1
---------------
 * Fixed crash in disassembly window when no corresponding source code is found in PDB

Version 0.2.4.0
---------------
 * Disassembly window was updated to match functionality of text windows. The window now allows
 toggling breakpoints, run to cursor, selecting lines, and so forth.
 * Fixed bug in disassembly prev/next handling. Moved Prev/Next buttons for exploring disassembly
 into context menu.
 * Watch, memory, and disassembly windows will now remain opened after the target has stopped.
 * Added date/time to process exit code in Output window for consistency

Version 0.2.3.0
---------------
 * Fixed bug matching line information with symbols in certain cases.
 * Added an option for tab spacing in Style menu.
 * Fixes to properly handle AltGr in various hotkeys.
 * Removed useless tooltips for panels (e.g., the tooltip for "Breakpoints" was "Breakpoints").
 * Call stack verbosity / register category state are now persisted.
 * No longer render continuously if RemedyBG does not have an active window.
 * The PDBs generated by Clang preserve the case of filenames (unlike Visual Studio). This has now
 been accounted for.
 * Fixed bug in automatic switching to main thread when control-break is used.
 * Expressions can now be used in the memory window.
 * Now remove existing breakpoints when opening a new session.
 * Disassembly window no longer takes focus upon appearing.

Version 0.2.2.1
---------------
 *  Fixed crash in handling of file checksums introduced in previous version.

Version 0.2.2.0
---------------
 * Added basic selection of text windows foreground and background color. These can be set in the
 Windows > Style menu (previously Windows > Fonts)
 * File open dialog's initial directory now matches current file's directory rather than using the
 system's directory
 * Tool tips for text window tabs now show the entire path
 * Added right-click context menu for text window tabs for copying the path to the clipboard
 * Fixed computation of selection rectangle in text window
 * Watch child nodes now use a stable indexing scheme. This fixes a bug where expanding one node
 could affect the expanded state of another node.
 * Now display a warning at the top of a text window if the file does not match the checksum stored
 in a symbol file.
 * Added stop debugging / restart menu items to the Control menu (repeated menu items already found
 under the Session menu).
 * We now properly preserve the show state of registers, memory, and disassembly windows across
 runs.
 * Memory window prev/next buttons now snap to top/bottom of window. This makes it easier to view
 memory that that straddles a page boundary.

Version 0.2.1.0
---------------
 * Now allow referencing registers (RAX, RSP, EAX, AL, etc.) in the watch window
 * Adds hexadecimal format specifier ('x' or 'X')
 * Memory window now refreshes with changes to target's state
 * Fixed typo in session state display for F11
 * Control>Break will default to the main thread rather than the OS created thread
 * Modules are now listed in the order they were loaded
 * Expression parser implements better handling of pointer arithmetic
 * Expression parse now allows casting an integer to an arbitrary type
 * No longer switch focus to text window if disassembly in focus
 * Fixed disassembly output when an invalid instruction is found in instruction stream
 * Colors for status bar now reflect exception vs. running vs. suspended state.

Version 0.2.0.1
---------------
 * Fixes a problem reading line information from the symbol file in certain cases. 

Version 0.2.0.0
---------------
 * Adds much more robust and powerful expression parser.
 * More robust handling of step-over and step-out in recursive functions.
 * Better handling when opening files in a session that no longer exist.
 * Run to cursor would sometimes run to the next line when using right-click menu. This has been
 fixed.
 * Run to cursor popup no longer affects the text window selection color.
 * Status text is now displayed on the same line as the menu.
 * Fixed indentation amount in the watch window for pointer to structures.
 * Names for pointer to pointer values now display a dereferenced expression rather than the empty
 string.
 * Watch window is now disabled while the target is executing.
 * Fixed potential crash when editing bitfield expression in watch.
 * Now properly invalidate the thread context when stepping.
 * Fixed parameter resolution for types larger than 8 bytes
 * Adds message to Output window in the case that a file cannot be opened.

Version 0.1.9.6
---------------
 * Fixed a crash in PDB line info handling when a segment's offset is zero.

Version 0.1.9.5
---------------
 * F10 / F11 will now step over/in by instruction whenever the disassembly window is in focus
 * Text windows no longer steal focus whenever the current line is updated.
 * Fixed a bug where the disassembly window wouldn't get updated after restarting the application in
 certain cases

Version 0.1.9.4
---------------
 * Fixed problem with step-in handling which caused the user to have to perform the operation twice.

Version 0.1.9.3
---------------
 * Fixed bug where newly opened text window wouldn't scroll the appropriate line
 * Current frame indicator now updated properly whenever target's state changes
 * No longer automatically jump to a function breakpoint
 * Size of function breakpoint modal window is now based on size of overall window (was too small on
 some high DPI monitors
 * Fixed a bug when trying to add a transient breakpoint to the same line twice (e.g.,
 run-to-cursor) 
 * Removed unused space at top of disassembly window
 * Changing frame or current thread now require double-click, rather than single-click
 * Added detailed information on why the target is executing or suspended
 * Fixed bug in watch window where multiple rows of a structure could be highlighted simultaneously
 * Removed insta-watch tooltips until the feature is more solid
 * Removed various buttons that were taking up room and replaced them with right-click context menu,
 instead
 * Shift+Enter now works like F2 in the watch window

Version 0.1.9.2
---------------
 * Fixed crash in symbol loading in cases where one or more of the sections had invalid symbol
 information.
 * Fixes high CPU usage when minimizing RemedyBG
 * Fixed crashed in symbol handling for i) opaque pointer types and ii) void* pointer types.

Version 0.1.9.1
---------------
 * PDBs searched in the EXE's directory are now properly closed after reading their signature.

Version 0.1.9.0
---------------
 * Full path is now stored in recent session list; fixed a bug when loading session file from the
 command line
 * Fixed issue in debugger engine where primitive pointer types (e.g., char*) were not being treated
 as pointers in certain situations.
 * Added string preview for character arrays
 * Watch window now respects current frame selected in call stack window. Now draw indicators in
 text / disassembly windows of the current frame's location.
 * Add newline after exception output
 * Fixed slow down of step-over / step-in operations when stepping over the REP/REPE/REPNE class of
 instructions.
 * Allow specifying frame select prefix for watch expressions (e.g., "{^1}" or "{^*}")
 * Can now lock an expression in the watch window to its current scope
 * Properly save off and restore maximized state of window
 * Added insta-watch tooltips in text windows.

Version 0.1.8.1
---------------
 * Fixes high GPU usage when the application is minimized

Version 0.1.8.0
---------------
 * Fixed crash in application breakpoint handling in cases when two or more breakpoints resolve to
 the same location
 * Added recent session list
 * If the target is not running, Ctrl+F10 will now launch the target and then run to the current
 line
 * Exception text is now written to the Output window
 * RemedyBG will take focus upon an exception in the target
 * Added hotkey Ctrl+Shift+X for breaking application
 * Added support for bitfields in watch window
 * Fixed correlation of symbol / line information in certain cases
 * Expression parser enhancements
   * Added address operator ('&') for getting an address of a variable
   * Supports casting between different pointer types
   * Adds format specifier to allow specifying number of array elements
 * Fixed modal function breakpoint dialog's font

Version 0.1.7.0
---------------
 * Fixed potential crash when symbol hash table size is zero.
 * If a canonical entry point cannot be found we now fall back to the entry point as specified in
 the PE header.
 * Fixed a crash when expanding a watch that has an empty subexpression name (e.g., void**)
 * Exception indicator is now properly cleared in the text window when target is stopped
 * Implement Run To Cursor feature (right-click in text window or Ctrl+F10)
 * Added global variable lookups to watch
 * In the watch window, if a local variable has been successfully resolved and then goes out of
 scope, we will continue to display this value (with grayed text).
 * Better text window docking and scrolling behavior; removed cursor from text window; horizontal
 scroll bar is now computed over full length of text rather than the just the portion of text on the
 screen.
 * Change to ensure that using the AltGR modifier does not cause focus to change to a menu.
 * Highlight suspended/executing indicator in menu bar. 
 * Fixed a bug that prevented user from scrolling in the Output window.
 * We now display the default working directory (matches executable's directory) in the Session
 menu.

Version 0.1.6.1
---------------
 * Fixed crash caused by setting "Command" to a relative, rather than absolute, path.
 * Added support for high DPI displays

Version 0.1.6.0
---------------
 * Reworked watch window user interface code. This includes support for keyboard controls.
 * Watch window structure/pointer dereferencing and array indexing
 * If the PDB that is specified in the image (EXE/DLL) is not found on disk, then we scan the
 image's directory for the first (and most likely only) PDB with a matching GUID and use that
 instead.
 * Removed ImGui's assertions from a release configuration. This fixes a dialog box that pops up if the configured font cannot be found on disk.
 * Text windows are now reloaded upon change.

Version 0.1.5.0
---------------
 * Added thread name to Thread window
 * Huge amount of work went into polishing up breakpoints
    * Now obtain the range of lines so we can draw a brace indicator in text window when toggling a
    breakpoint
    * Better handling of duplicate breakpoints
    * Fixed a thread-safety issue when opening text windows from debug engine thread
    * Added Ctrl+F9 to disable a breakpoint from text window
    * Now draw an indicator for deferred breakpoints (question mark).
    * Now properly resolve deferred breakpoints when a DLL is loaded.
 * Restored UI render loop to run as intended. This eliminates a number of rendering artifacts that
 were reported in the previous build.
 * The RemedyBG window now takes focus whenever we hit a breakpoint or if the process becomes
 suspended
 * Added window and text indicators for exception (first chance and unhandled).
 * Better error message when loading unsupported 32-bit executables.
 * ESC now closes Breakpoint / Exception windows.
 * Watch window show state now defaults to true
 * Added Call Stack verbosity options: Minimal, Top Frame Detailed, and Detailed
 * Reduced window padding and unnecessary menu bar in text windows
 * Default working directory, if one is not specified, is now the EXE's directory.
 * Upgrade to watch window input handling

Version 0.1.4.0
---------------
Thanks for all the feedback, folks!
 * Dear ImGui's INI file, along with some auxiliary files for storing window position and font
 information, is now located in the current user's application directory.
 * Version number in EXE now matches version in Help menu.
 * Vertex/pixel shaders for ImGui are now stored as blobs in the EXE to eliminate a dependency on
 D3DCompiler.
 * Beefed up initialization of DirectX so that RemedyBG will start properly on virtual machines.
 * Fixed a crash, triggered more readily on Windows 7, when reading the PE header from an image.
 * Now have the ability to set the UI font and Text font, along with their pixel sizes.
 * Removed embedded font from EXE. This helps reduce the size of the EXE.

Version 0.1.3.0
---------------
 * Internal size computation for union types has been fixed
 * Addresses a crash in watch window for a target's uninitialized pointer to a structure
 * Fixes minor typo in UI (missing parenthesis)

Version 0.1.2.0
---------------
 * Adds support for boolean types emitted by compiler (bool8, bool16, bool32, and bool64)
 * Fixes a memory allocation error for disassembled instruction checkpoint addresses.
 * Rewrote call stack display. In particular, the type names / type display now share the same code
 used for watches.

Version 0.1.1.0
---------------
 * Fixed a mismatch with the release version of XED that could potentially cause a crash

Version 0.1.0.0 - the Greek letter before Alpha
-----------------------------------------------
 * Initial release

