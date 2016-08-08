#
# Command module definition for the Dis VM shell
#

# Sys and Draw modules must be included before this file in modules.

Command: module
{
    init: fn(cxt: ref Draw->Context, args: list of string);
};
