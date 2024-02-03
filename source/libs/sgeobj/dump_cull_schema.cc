#include <stdio.h>
#include <stdlib.h>

#include <cjson/cJSON.h>

#include "cull/cull.h"

#include "uti/sge_rmon.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_object.h"

static void
add_string_to_array(cJSON *json_array, const char *name, const char *str)
{
   cJSON *json_array_item = cJSON_CreateObject();
   cJSON_AddItemToArray(json_array, json_array_item);
   cJSON_AddStringToObject(json_array_item, name, str);
}

static char *
get_type_name_from_descr(const lDescr *descr)
{
   char *type_name = nullptr;

   if (descr != nullptr) {
      const char *first_nm_str = lNm2Str(descr[0].nm);
      type_name = strdup(first_nm_str);
      char *pos = strchr(type_name, '_');
      if (pos != nullptr) {
         *pos = '\0';
      }
   }
   return type_name;
}

static void
print_flags_add(cJSON *json_flags, const char *flag)
{
   cJSON *json_flag = cJSON_CreateObject();
   cJSON_AddItemToArray(json_flags, json_flag);
   cJSON_AddStringToObject(json_flag, "name", flag);
}

static void
print_flags(int mt, cJSON *json_attribute)
{
   cJSON *json_flags = cJSON_AddArrayToObject(json_attribute, "flags");
   if (mt & CULL_PRIMARY_KEY) {
      print_flags_add(json_flags, "PRIMARY_KEY");
   }
   if (mt & CULL_UNIQUE) {
      print_flags_add(json_flags, "UNIQUE");
   }
   if (mt & CULL_HASH) {
      print_flags_add(json_flags, "HASH");
   }
   if (mt & CULL_CONFIGURE) {
      print_flags_add(json_flags, "CONFIGURE");
   }
   if (mt & CULL_JGDI_CONF) {
      print_flags_add(json_flags, "JGDI_CONF");
   }
   if (mt & CULL_JGDI_HIDDEN) {
      print_flags_add(json_flags, "JGDI_HIDDEN");
   }
   if (mt & CULL_JGDI_RO) {
      print_flags_add(json_flags, "JGDI_RO");
   }
   if (mt & CULL_SPOOL) {
      print_flags_add(json_flags, "SPOOL");
   }
   if (mt & CULL_SUBLIST) {
      print_flags_add(json_flags, "SPOOL_SUBLIST");
   }
   if (mt & CULL_SPOOL_PROJECT) {
      print_flags_add(json_flags, "SPOOL_PROJECT");
   }
   if (mt & CULL_SPOOL_USER) {
      print_flags_add(json_flags, "SPOOL_USER");
   }
}

static const char *
strip_prefix(char *string)
{
   const char *name = strchr(string, '_');
   return ++name;
}

static void
dump_schema(const lNameSpace *ns, const char *target_dir, cJSON *json_all)
{
   const lDescr *descr = ns->descr;
   char *obj_type = get_type_name_from_descr(descr);
   if (obj_type != nullptr) {
      int i;
      cJSON *json = cJSON_CreateObject();
      cJSON *json_attributes;
      cJSON *json_descr;

      printf("Object Type: %s\n", obj_type);
      // add type to json_all
      cJSON_AddStringToObject(json_all, "className", obj_type);
      cJSON_AddStringToObject(json_all, "cullPrefix", obj_type);
      cJSON_AddStringToObject(json_all, "cullNameSpec", "unknown");
      cJSON_AddNumberToObject(json_all, "size_in_basic_units",
                              MIN(ns->size / BASIC_UNIT + 2, MAX_DESCR_SIZE / BASIC_UNIT));
      cJSON_AddBoolToObject(json_all, "add_to_sge_all_lists", true);

      // add type + attributes to type specific file
      cJSON_AddStringToObject(json, "className", obj_type);
      cJSON_AddStringToObject(json, "summary", "@todo add summary");
      json_descr = cJSON_AddArrayToObject(json, "description");
      add_string_to_array(json_descr, "line", "@todo add description");
      cJSON_AddStringToObject(json, "cullPrefix", obj_type);
      json_attributes = cJSON_AddArrayToObject(json, "attributes");
      for (i = 0; i < ns->size; i++) {
         char *cull_name = strdup(ns->namev[i]);
         const char *name = strip_prefix(cull_name);
         if (name != nullptr && *name != '\0') {
            int data_type = mt_get_type(descr[i].mt);
            const char *data_type_str = lMt2Str(data_type);
            char *sub_obj_type = nullptr;
            cJSON *json_attribute = cJSON_CreateObject();
            cJSON_AddItemToArray(json_attributes, json_attribute);
            cJSON_AddStringToObject(json_attribute, "name", name);
            cJSON_AddStringToObject(json_attribute, "summary", "@todo add summary");
            json_descr = cJSON_AddArrayToObject(json_attribute, "description");
            add_string_to_array(json_descr, "line", "@todo add description");
            cJSON_AddStringToObject(json_attribute, "type", data_type_str);

            if (data_type == lListT || data_type == lObjectT || data_type == lRefT) {
               sub_obj_type = get_type_name_from_descr(object_get_subtype(descr[i].nm));
               if (sub_obj_type == nullptr) {
                  sub_obj_type = strdup("ANY");
               }
               cJSON_AddStringToObject(json_attribute, "subClassName", sub_obj_type);
               cJSON_AddStringToObject(json_attribute, "subCullPrefix", sub_obj_type);
            }
            print_flags(descr[i].mt, json_attribute);
            sge_free(&sub_obj_type);
         }
         sge_free(&cull_name);
      }
      {
         const char *json_str = cJSON_Print(json);
         char filename[SGE_PATH_MAX];
         FILE *fp;
         sprintf(filename, "%s/%s.json", target_dir, obj_type);
         fp = fopen(filename, "w");
         fwrite(json_str, strlen(json_str), 1, fp);
         fclose(fp);
         // printf("%s\n", json_str);
         sge_free(&json_str);
      }
      cJSON_Delete(json);
   }

   sge_free(&obj_type);
}

static void
walk_nmv(const char *target_dir)
{
   int i;
   cJSON *json_all = cJSON_CreateObject();
   cJSON *json_descr;

   cJSON_AddStringToObject(json_all, "summary", "This file lists all types in the correct order");

   json_descr = cJSON_AddArrayToObject(json_all, "description");
   add_string_to_array(json_descr, "line", "The file contains an array of JSON objects.");
   add_string_to_array(json_descr, "line", "Every JSON object describes one object type.");
   add_string_to_array(json_descr, "line", "Per object type we store the following data:");
   add_string_to_array(json_descr, "line", "  - className - name of the type / class, e.g. Job");
   add_string_to_array(json_descr, "line", "  - cullPrefix - prefix used in cull objects, e.g. JB");
   add_string_to_array(json_descr, "line", "  - size_in_basic_units - size forseen for the object type in the name space");
   add_string_to_array(json_descr, "line", "  - add_to_sge_all_lists - whether to add the type to the CULL sge_all_listsL.h");
   add_string_to_array(json_descr, "line", "    (needs to be set to false for internal types like PACK_Type to work around some dependency issues)");
   add_string_to_array(json_descr, "line", "    we set the size in chunks of BASIC_UNITs, a BASIC_UNIT is 50 entries");
   add_string_to_array(json_descr, "line", "    and leave ample space");

   cJSON *json_array = cJSON_AddArrayToObject(json_all, "types");

   // dump all types
   for (i = 0; nmv[i].descr != nullptr; i++) {
      cJSON *json_array_item;

      printf("Type %03d,\tlower: %d,\tsize: %d,\tfirst: %s\n",
             i, nmv[i].lower, nmv[i].size, nmv[i].namev[0]);

      json_array_item = cJSON_CreateObject();
      cJSON_AddItemToArray(json_array, json_array_item);
      dump_schema(&(nmv[i]), target_dir, json_array_item);
   }

   // dump the all types file
   {
      const char *json_str = cJSON_Print(json_all);
      char filename[SGE_PATH_MAX];
      FILE *fp;

      sprintf(filename, "%s/all_types.json", target_dir);
      fp = fopen(filename, "w");
      fwrite(json_str, strlen(json_str), 1, fp);
      fclose(fp);
      printf("%s\n", json_str);
      sge_free(&json_str);
   }

   cJSON_Delete(json_all);
}

int main(int argc, char *argv[])
{
   bool ret = true;
   const char *target_dir = nullptr;

   if (argc < 2) {
      fprintf(stderr, "usage: %s <target_dir>\n", argv[0]);
      ret = false;
   }

   if (ret) {
      target_dir = argv[1];
      printf("dumping json files to %s\n", target_dir);
   }

   if (ret) {
      lInit(nmv);
      walk_nmv(target_dir);
   }

   return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}
