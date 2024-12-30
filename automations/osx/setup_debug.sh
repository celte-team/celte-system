#!/bin/bash

# This script sets up auto core dumps on macOS for debugging purposes.
# Upon crashing, the application will generate a core dump file in /cores.
# Inspect the core dump file with LLDB to debug the crash.

# Enable core dumps and set the core dump file location
sudo sysctl -w kern.corefile=/cores/core.%P

# Persist the core dump file location setting
if ! grep -q "kern.corefile=/cores/core.%P" /etc/sysctl.conf; then
  echo "kern.corefile=/cores/core.%P" | sudo tee -a /etc/sysctl.conf
fi

# Create the core dump directory with appropriate permissions
sudo mkdir -p /cores
sudo chmod 1777 /cores

# Set the core dump size limit to unlimited
ulimit -c unlimited

# Persist the core dump size limit setting
if ! grep -q "ulimit -c unlimited" ~/.bash_profile; then
  echo "ulimit -c unlimited" >> ~/.bash_profile
fi

# Configure Crash Reporter to generate core dumps
CRASH_REPORTER_PLIST="/Library/Preferences/com.apple.CrashReporter.plist"
sudo defaults write $CRASH_REPORTER_PLIST CrashReporterKey -string "your-crash-reporter-key"
sudo defaults write $CRASH_REPORTER_PLIST AutoSubmit -bool false
sudo defaults write $CRASH_REPORTER_PLIST AutoSubmitVersion -int 4
sudo defaults write $CRASH_REPORTER_PLIST CrashReporterDialogType -string "Developer"

echo "Core dump setup complete. Core dumps will be saved to /cores/core.<pid>."

# Instructions for inspecting core dumps with LLDB
echo "To inspect a core dump with LLDB, use the following command:"
echo "lldb /path/to/your/application -c /cores/core.<pid>"