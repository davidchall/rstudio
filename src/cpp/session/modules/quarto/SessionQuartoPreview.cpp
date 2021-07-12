/*
 * SessionQuartoPreview.cpp
 *
 * Copyright (C) 2021 by RStudio, PBC
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

#include "SessionQuartoPreview.hpp"

#include <string>

#include <shared_core/Error.hpp>
#include <core/Exec.hpp>
#include <core/RegexUtils.hpp>
#include <core/json/JsonRpc.hpp>

#include <r/RExec.hpp>

#include <session/SessionModuleContext.hpp>

#include "SessionQuartoJob.hpp"

using namespace rstudio::core;
using namespace rstudio::session::module_context;

namespace rstudio {
namespace session {
namespace modules {
namespace quarto {
namespace preview {

namespace {

class QuartoPreview : public QuartoJob

{
public:
   static Error create(const core::FilePath& previewFile,
                       const std::string& format,
                       boost::shared_ptr<QuartoPreview>* ppPreview)
   {
      ppPreview->reset(new QuartoPreview(previewFile, format));
      return (*ppPreview)->start();
   }

   virtual ~QuartoPreview()
   {
   }

   FilePath previewFile()
   {
      return previewFile_;
   }

   std::string format()
   {
      return format_;
   }

   long port()
   {
      return port_;
   }

   Error render()
   {
      return r::exec::RFunction(".rs.quarto.renderPreview",
                                safe_convert::numberToString(port())).call();
   }

protected:
   explicit QuartoPreview(const FilePath& previewFile, const std::string& format)
      : QuartoJob(), previewFile_(previewFile), format_(format), port_(0)
   {
   }

   virtual std::string name()
   {
      return "Preview: " + previewFile_.getFilename();
   }

   virtual std::vector<std::string> args()
   {
      // preview target file
      std::vector<std::string> args({"preview"});
      args.push_back(string_utils::utf8ToSystem(previewFile_.getFilename()));

      // no automatic render and no browser
      args.push_back("--no-render");
      args.push_back("--no-browse");

      return args;
   }

   virtual core::FilePath workingDir()
   {
      return previewFile_.getParent();
   }


private:

   virtual void onStdErr(const std::string& output)
   {
      // detect browse directive
      if (port_ == 0) {
         auto location = quartoServerLocationFromOutput(output);
         if (location.port > 0)
         {
            // save port and path
            port_ = location.port;
            path_ = location.path;

            // show in viewer
            showInViewer();

            // restore the console tab after render
            activateConsole();

            // emit filtered output if we are on rstudio server
            if (session::options().programMode() == kSessionProgramModeServer)
            {
               QuartoJob::onStdErr(location.filteredOutput);
            }
            else
            {
               QuartoJob::onStdErr(output);
            }
            return;
         }
      }

      if (output.find("Watching files for changes") != std::string::npos)
      {
         // activate the console
         activateConsole();

         // if the viewer is already on the site just activate it
         if (boost::algorithm::starts_with(
                module_context::viewerCurrentUrl(false), viewerUrl()))
         {
            module_context::activatePane("viewer");
         }
         else
         {
            showInViewer();
         }
      }

      // standard output forwarding
      QuartoJob::onStdErr(output);
   }

   void activateConsole()
   {
      ClientEvent activateConsoleEvent(client_events::kConsoleActivate, false);
      module_context::enqueClientEvent(activateConsoleEvent);
   }

   void showInViewer()
   {
      module_context::viewer(viewerUrl(), false,  -1);
   }

   std::string viewerUrl()
   {
      return "http://localhost:" + safe_convert::numberToString(port_) + "/" + path_;
   }

private:
   FilePath previewFile_;
   std::string format_;
   long port_;
   std::string path_;
};


// keep a list of previews so we can re-render, re-activate
std::vector<boost::shared_ptr<QuartoPreview>> s_previews;

// preview singleton
boost::shared_ptr<QuartoPreview> s_pPreview;

// stop any running preview
void stopPreview()
{
   if (s_pPreview)
   {
      // stop the job if it's running
      if (s_pPreview->isRunning())
         s_pPreview->stop();

      // remove the job (will be replaced by a new quarto serve)
      s_pPreview->remove();
   }
}

// create a preview job
Error createPreview(const FilePath& previewFilePath, const std::string& format)
{
   // stop any running preview
   stopPreview();

   Error error = QuartoPreview::create(previewFilePath, format, &s_pPreview);
   if (error)
      return error;
   return Success();
}


Error quartoPreviewRpc(const json::JsonRpcRequest& request,
                       json::JsonRpcResponse*)
{
   // read params
   std::string previewFile, format;
   Error error = json::readParams(request.params, &previewFile, &format);
   if (error)
      return error;
   FilePath previewFilePath = module_context::resolveAliasedPath(previewFile);

   if (s_pPreview && s_pPreview->isRunning() &&
       (s_pPreview->previewFile() == previewFilePath) &&
       (s_pPreview->format() == format))
   {
      module_context::enqueClientEvent(ClientEvent(client_events::kJobsActivate));
      return s_pPreview->render();
   }
   else
   {
      return createPreview(previewFilePath, format);
   }
}


} // anonymous namespace



Error initialize()
{
   // register rpc functions
  ExecBlock initBlock;
   initBlock.addFunctions()
     (boost::bind(module_context::registerRpcMethod, "quarto_preview", quartoPreviewRpc))
   ;
   return initBlock.execute();
}

} // namespace prevew
} // namespace quarto
} // namespace modules
} // namespace session
} // namespace rstudio