/*
 * SessionConsoleProcessInfo.hpp
 *
 * Copyright (C) 2009-17 by RStudio, Inc.
 *
 * Unless you have received this program directly from RStudio pursuant
 * to the terms of a commercial license agreement with RStudio, then
 * this program is licensed to you under the terms of version 3 of the
 * GNU Affero General Public License. This program is distributed WITHOUT
 * ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THOSE OF NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Please refer to the
 * AGPL (http://www.gnu.org/licenses/agpl-3.0.txt) for more details.
 *
 */
#ifndef SESSION_CONSOLE_PROCESS_INFO_HPP
#define SESSION_CONSOLE_PROCESS_INFO_HPP

#include <boost/circular_buffer.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <core/json/Json.hpp>

namespace rstudio {
namespace core {
   class Error;
}
}

namespace rstudio {
namespace session {
namespace console_process_info {

enum InteractionMode
{
   InteractionNever = 0,
   InteractionPossible = 1,
   InteractionAlways = 2
};

extern const int kDefaultMaxOutputLines;
extern const int kDefaultTerminalMaxOutputLines;
extern const int kNoTerminal;

// ConsoleProcess metadata that is persisted, and sent to the client on
// create/reconnect
class ConsoleProcessInfo : boost::noncopyable,
                           public boost::enable_shared_from_this<ConsoleProcessInfo>
{
private:
   // This constructor is only for resurrecting orphaned processes (i.e. for
   // suspend/resume scenarios)
   ConsoleProcessInfo();

public:
   ConsoleProcessInfo(
         const std::string& caption,
         const std::string& title,
         const std::string& handle,
         const int terminalSequence,
         bool allowRestart,
         bool dialog,
         InteractionMode mode,
         int maxOutputLines = kDefaultMaxOutputLines);

   virtual ~ConsoleProcessInfo() {}

   // Caption is shown on terminal tabs, e.g. Terminal 1
   void setCaption(std::string& caption);
   std::string getCaption() const { return caption_; }

   // Title is set by terminal escape sequence, typically to show current dir
   void setTitle(std::string& title);
   std::string getTitle() const { return title_; }

   // Handle client uses to refer to this process
   void setHandle(std::string& handle);
   std::string getHandle() const { return handle_; }

   // Sequence number of the associated terminal; used to control display
   // order of terminal tabs; constant 'kNoTerminal' indicates a non-terminal
   void setTerminalSequence(int sequence);
   int getTerminalSequence() const { return terminalSequence_; }

   // Whether a ConsoleProcess object should start a new process on resume after
   // its process has been killed by a suspend.
   void setAllowRestart(bool allowRestart);
   bool getAllowRestart() const { return allowRestart_; }

   // Whether process is being associate with a dialog
   // TODO (gary) review if we still need this
   void setDialog(bool dialog);
   bool getDialog() const { return dialog_; }

   void setInteractionMode(InteractionMode mode);
   InteractionMode getInteractionMode() const { return interactionMode_; }

   void setMaxOutputLines(int maxOutputLines);
   int getMaxOutputLines() const { return maxOutputLines_; }

   void setShowOnOutput(bool showOnOutput);
   int getShowOnOutput() const { return showOnOutput_; }

   // Buffer output in case client disconnects/reconnects and needs
   // to recover some history. Used for modal dialog shell, not terminal
   // tabs.
   void appendToOutputBuffer(const std::string &str);
   std::string bufferedOutput() const;

   // Has the process exited, and what was the exit code?
   void clearExitCode();
   void setExitCode(int exitCode);
   boost::optional<int> getExitCode() const { return exitCode_; }

   // Whether the process has been successfully started
   void setIsStarted(bool started);
   bool isStarted() const { return started_; }

   // Does this process have child processes?
   void setHasChildProcs(bool hasChildProcs);
   bool getHasChildProcs() const { return childProcs_; }
   core::json::Object toJson() const;
   static boost::shared_ptr<ConsoleProcessInfo> fromJson(core::json::Object& obj);

private:
   std::string caption_;
   std::string title_;
   std::string handle_;
   int terminalSequence_;
   bool allowRestart_;
   bool dialog_;
   InteractionMode interactionMode_;
   int maxOutputLines_;
   bool showOnOutput_;
   boost::circular_buffer<char> outputBuffer_;
   boost::optional<int> exitCode_;
   bool started_;
   bool childProcs_;
};

} // namespace console_process_info
} // namespace session
} // namespace rstudio

#endif // SESSION_CONSOLE_PROCESS_INFO_HPP
