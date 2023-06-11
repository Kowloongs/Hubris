/* Copyright (C) 2019 MariaDB Corporation

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA. */

#include "TruncateTask.h"
#include <errno.h>
#include "messageFormat.h"
#include "SMLogging.h"

using namespace std;

namespace storagemanager
{
TruncateTask::TruncateTask(int sock, uint len) : PosixTask(sock, len)
{
}

TruncateTask::~TruncateTask()
{
}

#define check_error(msg, ret) \
  if (success < 0)            \
  {                           \
    handleError(msg, errno);  \
    return ret;               \
  }

bool TruncateTask::run()
{
  SMLogging* logger = SMLogging::get();
  int success;
  uint8_t buf[1024] = {0};

  if (getLength() > 1023)
  {
    handleError("TruncateTask read", ENAMETOOLONG);
    return false;
  }

  success = read(buf, getLength());
  check_error("TruncateTask read", false);
  truncate_cmd* cmd = (truncate_cmd*)buf;

#ifdef SM_TRACE
  logger->log(LOG_DEBUG, "truncate %s newlength %i.", cmd->filename, cmd->length);
#endif
  int err;

  try
  {
    err = ioc->truncate(cmd->filename, cmd->length);
  }
  catch (exception& e)
  {
    logger->log(LOG_ERR, "TruncateTask: caught '%s'", e.what());
    errno = EIO;
    err = -1;
  }
  if (err)
  {
    handleError("TruncateTask truncate", errno);
    return true;
  }

  sm_response* resp = (sm_response*)buf;
  resp->returnCode = 0;
  return write(*resp, 0);
}

}  // namespace storagemanager
