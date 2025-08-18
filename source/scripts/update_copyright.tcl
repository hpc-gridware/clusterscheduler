#!/usr/bin/tclsh
#___INFO__MARK_BEGIN_NEW__
###########################################################################
#
#  Copyright 2025 HPC-Gridware GmbH
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
###########################################################################
#___INFO__MARK_END_NEW__

#
# this scripts takes a list of file names and inserts or updates copyright headers
#
# we have the following markers for copyright sections:
# - __INFO__MARK_BEGIN__         : existing file with SISSL header coming from the Grid Engine open source repository
# - __INFO__MARK_BEGIN_NEW__     : new file in one of the cluster scheduler repositories which are open source
# - __INFO__MARK_BEGIN_CLOSED__  : new file in one of the closed cluster scheduler repositories
#
# Checks:
# - is the file in git - if not: report and no action
# - does the file have a copyright header at all (one of the ___INFO_MARK markers exists), if not: report and no action
# - for ___INFO__MARK_BEGIN__: must be a SISSL header
# - for ___INFO__MARK_BEGIN_NEW__:
#    - if a copyright section exists then it has to be Apache ....
#    - if it does not exist than add it
# - for ___INFO__MARK_BEGIN_CLOSED and ___INFO__MARK_END_CLOSED__: may only be a new file (since December 2023)

# Call it for all files containing copyright notices, e.g.
# update_copyright.tcl $H $CC
# update_copyright.tcl source/dist/util/resources/schemas/*/*.xsd
# update_copyright.tcl container/*/Dockerfile container/*/Makefile

# main

proc main {} {
   global argc argv0 argv
   global add_current_year

   set do_dryrun 0
   set add_current_year 0

   if {$argc < 1} {
      puts "usage: $argv0 \[-n\] \[--with-now\] file1 \[file2 \[... filen\]\]"
      puts "   -n : do a dryrun"
      puts "   --with-now : add current year to copyright header"
      exit 1
   }

   foreach filename $argv {
      if {$filename eq "-n"} {
         set do_dryrun 1
         continue
      }
      if {$filename eq "--with-now"} {
         set add_current_year 1
         continue
      }

      if {[exclude_filename $filename]} {
         continue
      }
      if {![file exists $filename]} {
         puts stderr "$filename: does not exist"
         continue
      }
      if {![file isfile $filename]} {
         puts stderr "$filename: is not a regular file"
         continue
      }
      if {![read_file $filename lines permissions]} {
         continue
      }
      set new_lines [process_file $filename lines]
      if {[llength $new_lines] > 0} {
         if {!$do_dryrun} {
            write_file $filename new_lines $permissions
         }
      }
      unset -nocomplain lines new_lines
   }
}

proc exclude_filename {filename} {
   set ret 0

   switch -glob $filename {
      "source/scripts/update_copyright.tcl" {
         # don't update ourselves - we have the copyright markers in many places
         set ret 1
      }
      "3rdparty/*" -
      "*/3rdparty/*" -
      "*cmake-build*" {
         set ret 1
      }
      "*sge_spooling_flatfile_scanner.cc" {
         set ret 1 ; # generated file
      }
      "*/CPM.cmake" {
         set ret 1 ;# not our IP
      }
   }

   return $ret
}

proc read_file {filename lines_var permissions_var} {
   upvar $lines_var lines
   upvar $permissions_var permissions

   unset -nocomplain lines ; set lines {}

   # try to open and read the file
   set f ""
   if {[catch {
      set f [open $filename "r"]
      set num_lines 0
      while {[gets $f line] >= 0} {
         lappend lines $line
         incr num_lines
      }
      close $f
   } catch_message]} {
      puts stderr "$filename: reading failed: $catch_message"
      # if the open worked but reading failed, we still have the file handle
      if {$f ne ""} {
         catch {close $f}
      }
      return 0
   }

   set permissions [file attributes $filename -permissions]

   puts "$filename: $permissions: read $num_lines lines"

   return 1
}

proc write_file {filename lines_var permissions} {
   upvar $lines_var lines

   puts "$filename: writing new version"

   # make a backup
   file rename $filename "$filename.bak"

   set catch_result [catch {
      set f [open $filename "w" $permissions]
      foreach line $lines {
         puts $f $line
      }
      close $f
   } catch_message]

   if {$catch_result != 0} {
      puts stderr "$filename: writing failed: $catch_message"
      # error: restore original file
      file delete $filename
      file rename "$filename.bak" $filename
   } else {
      # success: delete backup
      file delete "$filename.bak"
   }
}

proc process_file {filename lines_var} {
   upvar $lines_var lines

   set new_lines {}

   # get copyright header and comment type
   if {![get_copyright_header $filename lines copyright copyright_type comment_type start_index end_index]} {
      return $new_lines
   }
   puts "$filename: $comment_type comment header with $copyright_type license from line $start_index to $end_index"

   set added_new_header 0
   if {![check_copyright_header $filename copyright copyright_type comment_type added_new_header]} {
      return $new_lines
   }
   puts "$filename: header OK"

   if {[add_update_gridware_copyright $filename copyright $copyright_type $comment_type] ||
       $added_new_header} {
      # header has been changed
      puts "$filename header updated"
      # @todo rename copyright to header

      set skip 0
      # go over all lines copy them and insert new header
      foreach line $lines {
         if {$skip} {
            if {[string first "___INFO__MARK_END_" $line] >= 0} {
               # copy end mark
               lappend new_lines $line
               set skip 0
            }
         } else {
            lappend new_lines $line

            if {[string first "___INFO__MARK_BEGIN_" $line] >= 0} {
               # found start mark, copy new header
               foreach new_line $copyright {
                  lappend new_lines $new_line
               }
               # skip lines of the old header
               set skip 1
            }
         }
      }
   }

   return $new_lines
}

proc get_copyright_header {filename lines_var header_var copyright_type_var comment_type_var start_index_var end_index_var} {
   upvar $lines_var lines
   upvar $header_var header ; set header {}
   upvar $copyright_type_var copyright_type
   upvar $comment_type_var comment_type
   upvar $start_index_var start_index
   upvar $end_index_var end_index

   set ret 1

   set header_started 0
   set header_ended 0
   set idx 0
   foreach line $lines {
      # expect clean marker lines without additional characters
      if {!$header_started} {
         switch $line {
            "#___INFO__MARK_BEGIN__" {
               set copyright_type "SISSL"
               set comment_type "script"
               set header_started 1
               set start_index $idx
            }
            "#___INFO__MARK_BEGIN_NEW__" {
               set copyright_type "NEW"
               set comment_type "script"
               set header_started 1
               set start_index $idx
            }
            "#___INFO__MARK_BEGIN_CLOSED__" {
               set copyright_type "CLOSED"
               set comment_type "script"
               set header_started 1
               set start_index $idx
            }
            "/*___INFO__MARK_BEGIN__*/" {
               set copyright_type "SISSL"
               set comment_type "C"
               set header_started 1
               set start_index $idx
            }
            "/*___INFO__MARK_BEGIN_NEW__*/" {
               set copyright_type "NEW"
               set comment_type "C"
               set header_started 1
               set start_index $idx
            }
            "/*___INFO__MARK_BEGIN_CLOSED__*/" {
               set copyright_type "CLOSED"
               set comment_type "C"
               set header_started 1
               set start_index $idx
            }
         }
      } else {
         # within header
         # check for header end, we expect it not to start on column 0, there must be some comment line
         # as long as we do not find the header end, copy the header
         if {[string first "___INFO__MARK_END_" $line] > 0} {
            set header_ended 1
            set end_index $idx
         } else {
            lappend header $line
         }
      }

      # we are done with the header
      if {$header_ended} {
         break
      }

      incr idx
   }

   if {!$header_started} {
      puts stderr "$filename: no copyright header found"
      set ret 0
   } elseif {!$header_ended} {
      puts stderr "$filename: no end of copyright header found"
      set ret 0
   }

   return $ret
}

proc check_copyright_header {filename header_var copyright_type_var comment_type_var added_new_header_var} {
   upvar $header_var header
   upvar $copyright_type_var copyright_type
   upvar $comment_type_var comment_type
   upvar $added_new_header_var added_new_header

   set ret 1

   # add missing header
   if {[llength $header] == 0} {
      global apache_license closed_license

      if {$copyright_type == "NEW"} {
         puts "$filename: adding Apache header"
         set split_header [split $apache_license "\n"]
      } else {
         puts "$filename: adding Closed header"
         set split_header [split $closed_license "\n"]
      }

      if {$comment_type == "C"} {
         set header_start "/***************************************************************************"
         set header_end " ***************************************************************************/"
         set header_prefix " * "
      } else {
         set header_start "###########################################################################"
         set header_end "###########################################################################"
         set header_prefix "# "
      }

      lappend header $header_start
      foreach line $split_header {
         lappend header "$header_prefix $line"
      }
      lappend header $header_end

      set added_new_header 1
   } else {
      # check existing header
      if {$comment_type == "script"} {
         set comment_line "##########"
      } elseif {$comment_type == "C"} {
         set comment_line " **********"
      }
      if {[string first $comment_line [lindex $header end]] != 0} {
         puts stderr "$filename: last line of comment doesn't start with \"$comment_line\""
         set ret 0
      }
   }

   # for SISSL headers search for "Sun Industry Standards Source License"
   # NEW headers need to contain "Apache "
   if {$copyright_type == "SISSL"} {
      set license_name "Sun Industry Standards Source License"
   } elseif {$copyright_type == "NEW"} {
      set license_name "Apache "
   } else {
      set license_name ""
   }

   if {$license_name != ""} {
      set found_license_name 0
      foreach line $header {
         if {[string first $license_name $line] > 0} {
            set found_license_name 1
            break
         }
      }
      if {!$found_license_name} {
         puts stderr "$filename: $copyright_type header doesn't contain \"$license_name\""
      }
   }

   return $ret
}


proc add_update_gridware_copyright {filename copyright_var copyright_type comment_type} {
   upvar $copyright_var header

   set changed 0

   # get year-range
   set year_range [get_year_range_from_git $filename]
   if {$year_range == ""} {
      # we didn't do any changes to the header
      return $changed
   }

   # search HPC-Gridware copyright line
   set idx 0
   set found 0
   foreach line $header {
      if {[string match "*Copyright * HPC-Gridware*" $line]} {
         set backup $line
         set found 1
         break
      }
      incr idx
   }

   set line ""
   # start comment line
   switch $comment_type {
      "C" {
         set comment_start " *"
      }
      "script" {
         set comment_start "#"
      }
      default {
         puts stderr "invalid comment_type $comment_type"
      }
   }

   # if found: update it
   set line $comment_start
   if {$found} {
      if {[string first "Portions of this software are Copyright (c) " [lindex $header $idx]] >= 0} {
         append line "  Portions of this software are Copyright (c) "
      } else {
         append line "  Copyright "
      }
      append line $year_range
      append line " HPC-Gridware GmbH"
      if {$line != $backup} {
         set header [lreplace $header $idx $idx $line]
         set changed 1
      }
   } else {
      # if not found: add it
      if {$copyright_type == "SISSL"} {
         append line "  Portions of this software are Copyright (c) "
      } else {
         append line "  Copyright "
      }
      append line $year_range
      append line " HPC-Gridware GmbH"

      set header [linsert $header "end-1" $line $comment_start]
      set changed 1
   }

   # we did changed to the header
   return $changed
}

proc get_year_range_from_git {filename} {
   global add_current_year

   set ret ""

   # @todo need to call open and close in a catch block and check for error
   set years {}
   set f [open |[list git log --follow --since=2023 --pretty=format:%as\ %an $filename] "r"]
   while {[gets $f line] >= 0} {
      set year [lindex [split [lindex [split $line " "] 0] "-"] 0]
      lappend years $year
   }
   close $f

   if {$add_current_year} {
      # add current year
      set current_year [clock format [clock seconds] -format "%Y"]
      lappend years $current_year
   }

   if {[llength $years] > 0} {
      # there were changes since 2023
      set years [lsort -unique -integer -increasing $years]
      set ret [lindex $years 0]
      if {[llength $years] > 1} {
         append ret "-"
         append ret [lindex $years end]
      }
   }

   return $ret
}

set apache_license "
Copyright 2025 HPC-Gridware GmbH

Licensed under the Apache License, Version 2.0 (the \"License\");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an \"AS IS\" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"

set closed_license "
Copyright 2025 HPC-Gridware GmbH
"

################################################################################
main
