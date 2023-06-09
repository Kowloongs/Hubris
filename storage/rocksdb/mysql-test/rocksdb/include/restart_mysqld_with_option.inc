
if ($rpl_inited)
{
  if (!$allow_rpl_inited)
  {
    --die ERROR IN TEST: This script does not support replication
  }
}

# Write file to make mysql-test-run.pl expect the "crash", but don't start
# it until it's told to
--let $_expect_file_name= `select regexp_replace(@@tmpdir, '^.*/','')`
--let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/$_expect_file_name.expect
--exec echo "wait" > $_expect_file_name

# Send shutdown to the connected server and give
# it 10 seconds to die before zapping it
shutdown_server;

# Write file to make mysql-test-run.pl start up the server again
--exec echo "restart:$_mysqld_option" > $_expect_file_name

# Turn on reconnect
--enable_reconnect

# Call script that will poll the server waiting for it to be back online again
--source include/wait_until_connected_again.inc

# Turn off reconnect again
--disable_reconnect

