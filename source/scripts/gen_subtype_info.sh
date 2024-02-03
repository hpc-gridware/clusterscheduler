#!/bin/sh

output=$1

echo "/* auto generated file */"             > $output
echo "#include <cstddef>"                    >>$output
echo "#include \"cull/cull.h\""              >>$output
echo "#include \"sgeobj/cull/sge_all_listsL.h\"" >>$output
echo ""                                      >>$output
echo "const lDescr *"                        >>$output
echo "object_get_subtype(int nm)"            >>$output
echo "{"                                     >>$output
echo "   const lDescr *ret = nullptr;"          >>$output
echo "   switch (nm) {"                      >>$output
while read dummy name type; do
   if [ $type != CULL_ANY_SUBTYPE ]; then
      echo "      case $name:"               >>$output
      echo "         ret = $type;"           >>$output
      echo "         break;"                 >>$output
   fi
done
echo "   }"                                  >>$output
echo "   return ret;"                        >>$output
echo "}"                                     >>$output
