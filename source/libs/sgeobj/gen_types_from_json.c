#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <cjson/cJSON.h>

#include "basis_types.h"
#include "uti/sge_dstring.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"

#define EXTERN_C_BEGIN "#ifdef __cplusplus\nextern \"C\" {\n#endif"
#define EXTERN_C_END "#ifdef __cplusplus\n}\n#endif"

static const char *
read_file(const char *dir, const char *name)
{
   char *ret = NULL;
   char filename[SGE_PATH_MAX];
   SGE_STRUCT_STAT statbuf;

   sprintf(filename, "%s/%s", dir, name);
   if (SGE_STAT(filename, &statbuf) == -1) {
      fprintf(stderr, "file %s does not exist\n", filename);
      // this is OK
   } else {
      size_t size = statbuf.st_size;
      char *buffer = sge_malloc(size + 1);
      FILE *fd = fopen(filename, "r");
      if (fd == NULL) {
         fprintf(stderr, "error opening file %s: %s\n", filename, strerror(errno));
      } else {
         size_t read_size = fread(buffer, size, 1, fd);
         if (read_size != 1) {
            fprintf(stderr, "error reading file %s: %s\n", filename, strerror(errno));
            ret = false;
         } else {
            buffer[size] = '\0';
            ret = buffer; // to be freed by the caller
            //printf("read file %s\n", filename);
         }
      }
   }
      
   return ret;
}

static cJSON *
read_json_from_file(const char *dir, const char *name)
{
   cJSON *ret = NULL;
   const char *str_json = read_file(dir, name);
   if (str_json == NULL) {
      fprintf(stderr, "no json to parse\n");
   } else {
      const char *errorPtr;
      ret = cJSON_Parse(str_json);
      errorPtr = cJSON_GetErrorPtr();
      if (errorPtr != NULL) {
         fprintf(stderr, "error parsing file %s: %s\n", name, errorPtr);
         cJSON_Delete(ret);
         ret = NULL;
      }
   }
   sge_free(&str_json);

   return ret;
}

const char *
get_json_string(cJSON *json, const char *attrib)
{
   cJSON *json_obj = cJSON_GetObjectItem(json, attrib);
   return cJSON_GetStringValue(json_obj);
}

int
get_json_number(cJSON *json, const char *attrib)
{
   cJSON *json_obj = cJSON_GetObjectItem(json, attrib);
   return cJSON_GetNumberValue(json_obj);
}

bool
get_json_bool(cJSON *json, const char *attrib)
{
   bool ret = false;
   cJSON *json_obj = cJSON_GetObjectItem(json, attrib);
   if (cJSON_IsTrue(json_obj)) {
      ret = true;
   }
   return ret;
}

static void
print_multiline_comment(dstring *dstr_doc, cJSON *json_array, const char *indent)
{
   int num_items = cJSON_GetArraySize(json_array);
   int i;

   for (i = 0; i < num_items; i++) {
      cJSON *json_item = cJSON_GetArrayItem(json_array, i);
      const char *line = get_json_string(json_item, "line");
      sge_dstring_sprintf_append(dstr_doc, "%s%s\n", indent, line);
   }
   sge_dstring_sprintf_append(dstr_doc, "*\n");
}

void
dump_cull_type_begin(dstring *dstr_begin, const char *source_dir, const char *target_dir,
                     const char *summary, cJSON *json_description)
{
   const char *str_copyright = read_file(source_dir, "copyright.txt");
   if (str_copyright != NULL) {
      sge_dstring_append(dstr_begin, str_copyright);
   }
   sge_free(&str_copyright);

   sge_dstring_sprintf_append(dstr_begin, "#include \"cull/cull.h\"\n");
   sge_dstring_sprintf_append(dstr_begin, "#include \"%s/sge_boundaries.h\"\n\n", target_dir);
   sge_dstring_sprintf_append(dstr_begin, "%s\n\n", EXTERN_C_BEGIN);
   sge_dstring_sprintf_append(dstr_begin, "/**\n* @brief %s\n*\n", summary);
   print_multiline_comment(dstr_begin, json_description, "* ");
}

const char *
dump_cull_map_type(const char *json_type)
{
   const char *ret;

   if (strcmp(json_type, "lFloatT") == 0) {
      ret = "SGE_FLOAT";
   } else if (strcmp(json_type, "lDoubleT") == 0) {
      ret = "SGE_DOUBLE";
   } else if (strcmp(json_type, "lUlongT") == 0) {
      ret = "SGE_ULONG";
   } else if (strcmp(json_type, "lLongT") == 0) {
      ret = "SGE_LONG";
   } else if (strcmp(json_type, "lCharT") == 0) {
      ret = "SGE_CHAR";
   } else if (strcmp(json_type, "lBoolT") == 0) {
      ret = "SGE_BOOL";
   } else if (strcmp(json_type, "lIntT") == 0) {
      ret = "SGE_INT";
   } else if (strcmp(json_type, "lStringT") == 0) {
      ret = "SGE_STRING";
   } else if (strcmp(json_type, "lListT") == 0) {
      ret = "SGE_LIST";
   } else if (strcmp(json_type, "lObjectT") == 0) {
      ret = "SGE_OBJECT";
   } else if (strcmp(json_type, "lRefT") == 0) {
      ret = "SGE_REF";
   } else if (strcmp(json_type, "lHostT") == 0) {
      ret = "SGE_HOST";
   } else if (strcmp(json_type, "lUlong64T") == 0) {
      ret = "SGE_ULONG64";
   } else {
      ret = "SGE_UNKNOWN";
   }

   return ret;
}

static bool
dump_cull_type_doc(cJSON *json_attrib, const char *cull_prefix, dstring *dstr_doc)
{
   bool ret = true;
   const char *name = get_json_string(json_attrib, "name");
   const char *summary = get_json_string(json_attrib, "summary");
   const char *type = dump_cull_map_type(get_json_string(json_attrib, "type"));
   const char *indent = "*    ";
   cJSON *json_description = cJSON_GetObjectItem(json_attrib, "description");

   sge_dstring_sprintf_append(dstr_doc, "%s%s(%s_%s) - %s\n", indent, type, cull_prefix, name, summary);
   print_multiline_comment(dstr_doc, json_description, indent);

   return ret;
}

static bool
dump_cull_type_enum(cJSON *json_attrib, const char *cull_prefix, dstring *dstr_enum, bool last)
{
   bool ret = true;
   const char *name = get_json_string(json_attrib, "name");

   sge_dstring_sprintf_append(dstr_enum, "   %s_%s%s\n", cull_prefix, name, last ? "" : ",");

   return ret;
}

const char *dump_cull_type_flags(cJSON *json_flags, dstring *dstr_flags)
{
   int num_items = cJSON_GetArraySize(json_flags);
   int i;

   sge_dstring_sprintf(dstr_flags, "CULL_DEFAULT");

   for (i = 0; i < num_items; i++) {
      cJSON *json_item = cJSON_GetArrayItem(json_flags, i);
      const char *name = get_json_string(json_item, "name");

      // dump flags except for the JGDI_*
      if (strstr(name, "JGDI") == NULL) {
         sge_dstring_sprintf_append(dstr_flags, ", %s", name);
      }
   }

   return sge_dstring_get_string(dstr_flags);
}

static bool
dump_cull_type_listdef(cJSON *json_attrib, const char *cull_prefix, dstring *dstr_listdef)
{
   bool ret = true;
   const char *name = get_json_string(json_attrib, "name");
   const char *type = dump_cull_map_type(get_json_string(json_attrib, "type"));
   dstring dstr_flags = DSTRING_INIT;
   cJSON *json_flags = cJSON_GetObjectItem(json_attrib, "flags");
   const char *flags = dump_cull_type_flags(json_flags, &dstr_flags);

   if (strcmp(type, "SGE_LIST") == 0 || strcmp(type, "SGE_OBJECT") == 0 || strcmp(type, "SGE_REF") == 0) {
      const char *sub_prefix = get_json_string(json_attrib, "subCullPrefix");
      if (sub_prefix == NULL || strcmp(sub_prefix, "ANY") == 0) {
         sge_dstring_sprintf_append(dstr_listdef, "   %s(%s_%s, CULL_ANY_SUBTYPE, %s)\n",
                                    type, cull_prefix, name, flags);
      } else {
         sge_dstring_sprintf_append(dstr_listdef, "   %s(%s_%s, %s_Type, %s)\n",
                                    type, cull_prefix, name, sub_prefix, flags);
      }
   } else {
      sge_dstring_sprintf_append(dstr_listdef, "   %s(%s_%s, %s)\n", type, cull_prefix, name, flags);
   }

   sge_dstring_free(&dstr_flags);

   return ret;
}

static bool
dump_cull_type_namedef(cJSON *json_attrib, const char *cull_prefix, dstring *dstr_namedef)
{
   bool ret = true;
   const char *name = get_json_string(json_attrib, "name");

   sge_dstring_sprintf_append(dstr_namedef, "   NAME(\"%s_%s\")\n", cull_prefix, name);

   return ret;
}

static bool
dump_cull_type_Lh(const char *source_dir, const char *target_dir, const char *cull_prefix, const char *target_filename)
{
   bool ret = true;

   dstring dstr_begin = DSTRING_INIT;
   dstring dstr_enum = DSTRING_INIT;
   dstring dstr_listdef = DSTRING_INIT;
   dstring dstr_namedef = DSTRING_INIT;

   cJSON *json;
   char source_filename[SGE_PATH_MAX];

   sprintf(source_filename, "%s.json", cull_prefix);
   json = read_json_from_file(source_dir, source_filename);
   if (json == NULL) {
      ret = false;
   }

   if (ret) {
      const char *summary = get_json_string(json, "summary");
      cJSON *json_description = cJSON_GetObjectItem(json, "description");

      cJSON *json_attribs = cJSON_GetObjectItem(json, "attributes");
      int num_attribs = cJSON_GetArraySize(json_attribs);
      int i;

      // add includes + documentation 
      dump_cull_type_begin(&dstr_begin, source_dir, target_dir, summary, json_description);
      // initialize sections
      sge_dstring_sprintf_append(&dstr_enum, "enum {\n");
      sge_dstring_sprintf_append(&dstr_listdef, "LISTDEF(%s_Type)\n", cull_prefix);
      sge_dstring_sprintf_append(&dstr_namedef, "NAMEDEF(%sN)\n", cull_prefix);

      for (i = 0; i < num_attribs; i++) {
         cJSON *json_attrib = cJSON_GetArrayItem(json_attribs, i);
         dump_cull_type_doc(json_attrib, cull_prefix, &dstr_begin);
         dump_cull_type_enum(json_attrib, cull_prefix, &dstr_enum, i == num_attribs -1);
         dump_cull_type_listdef(json_attrib, cull_prefix, &dstr_listdef);
         dump_cull_type_namedef(json_attrib, cull_prefix, &dstr_namedef);
      }

      // finalize sections
      sge_dstring_sprintf_append(&dstr_begin, "*/\n\n");
      sge_dstring_sprintf_append(&dstr_enum, "};\n\n");
      sge_dstring_sprintf_append(&dstr_listdef, "LISTEND\n\n");
      sge_dstring_sprintf_append(&dstr_namedef, "NAMEEND\n\n", cull_prefix);
      sge_dstring_sprintf_append(&dstr_namedef, "#define %sS sizeof(%sN)/sizeof(char *)\n\n",
                                 cull_prefix, cull_prefix);
      sge_dstring_sprintf_append(&dstr_namedef, "%s\n\n", EXTERN_C_END);
   }

   // dump to file
   if (ret) {
      FILE *fp = fopen(target_filename, "w");
      const char *str_begin = sge_dstring_get_string(&dstr_begin);
      const char *str_enum = sge_dstring_get_string(&dstr_enum);
      const char *str_listdef = sge_dstring_get_string(&dstr_listdef);
      const char *str_namedef = sge_dstring_get_string(&dstr_namedef);

      fprintf(fp, "#ifndef SGE_%s_L_H\n#define SGE_%s_L_H\n", cull_prefix, cull_prefix);
      fwrite(str_begin, strlen(str_begin), 1, fp);
      fwrite(str_enum, strlen(str_enum), 1, fp);
      fwrite(str_listdef, strlen(str_listdef), 1, fp);
      fwrite(str_namedef, strlen(str_namedef), 1, fp);
      fprintf(fp, "#endif\n");
      fclose(fp);
      //printf("wrote file %s\n", target_filename);
   }

   // cleanup
   cJSON_Delete(json);
   sge_dstring_free(&dstr_begin);
   sge_dstring_free(&dstr_enum);
   sge_dstring_free(&dstr_listdef);
   sge_dstring_free(&dstr_namedef);

   return ret;
}

static bool
dump_cull_type(cJSON *json_type, const char **last_cull_prefix,
               dstring *dstr_boundaries, dstring *dstr_all_types_includes, dstring *dstr_all_types_body,
               const char *source_dir, const char *target_dir)
{
   bool ret = true;

   const char *cull_prefix = get_json_string(json_type, "cullPrefix");
   const char *cull_name_spec = get_json_string(json_type, "cullNameSpec");
   int size = get_json_number(json_type, "size_in_basic_units");
   bool add_to_all_lists = get_json_bool(json_type, "add_to_sge_all_lists");
   char filename[SGE_PATH_MAX];

   printf("%s_Type\n", cull_prefix);

   // create boundaries.h content
   if (*last_cull_prefix == NULL) {
      sge_dstring_sprintf_append(dstr_boundaries, "   %s_LOWERBOUND = 1*BASIC_UNIT,\n", cull_prefix);
   } else {
      sge_dstring_sprintf_append(dstr_boundaries, "   %s_LOWERBOUND = %s_UPPERBOUND + 1,\n", cull_prefix, *last_cull_prefix);
   }
   sge_dstring_sprintf_append(dstr_boundaries, "   %s_UPPERBOUND = %s_LOWERBOUND + %d*BASIC_UNIT - 1,\n\n",
                              cull_prefix, cull_prefix, size);

   *last_cull_prefix = cull_prefix;

   // create all_listsL.h content
   sprintf(filename, "%s/sge_%s_%s_L.h", target_dir, cull_name_spec, cull_prefix);
   if (add_to_all_lists) {
      sge_dstring_sprintf_append(dstr_all_types_includes, "#include \"%s\"\n", filename);
      sge_dstring_sprintf_append(dstr_all_types_body, "   {%s_LOWERBOUND, %sS, %sN, %s_Type},\n",
                                  cull_prefix, cull_prefix, cull_prefix, cull_prefix);
   }

   ret = dump_cull_type_Lh(source_dir, target_dir, cull_prefix, filename);

   return ret;
}

static bool
dump_all_boundaries(dstring *dstr_boundaries, const char *source_dir, const char *target_dir)
{
   bool ret = true;
   /* prerequisits, read in
    *    - copyright text        - added in the beginning of every file
    */
   const char *str_copyright = read_file(source_dir, "copyright.txt");
   const char *str_boundaries = sge_dstring_get_string(dstr_boundaries);
   if (str_copyright == NULL || str_boundaries == NULL) {
      fprintf(stderr, "boundary input is not complete\n");
      ret = false;
   }

   if (ret) {
      char filename[SGE_PATH_MAX];
      FILE *fp;
      sprintf(filename, "%s/sge_boundaries.h", target_dir);
      fp = fopen(filename, "w");
      fputs("#ifndef __SGE_BOUNDARIES_H\n#define __SGE_BOUNDARIES_H\n", fp);
      fwrite(str_copyright, strlen(str_copyright), 1, fp);
      fwrite(str_boundaries, strlen(str_boundaries), 1, fp);
      fputs("#endif /* __SGE_BOUNDARIES_H */\n", fp);
      fclose(fp);
      //printf("wrote file %s\n", filename);
   }

   // cleanup
   sge_free(&str_copyright);

   return ret;
}

static const char *
dump_all_lists_pre(dstring *dstr_pre)
{
   sge_dstring_append(dstr_pre, "#include \"basis_types.h\"\n");
   sge_dstring_append(dstr_pre, "#include \"cull/cull.h\"\n\n");
   sge_dstring_append(dstr_pre, "/* Definition of new names */\n\n");

   return sge_dstring_get_string(dstr_pre);
}

static const char *
dump_all_lists_mid(dstring *dstr_mid)
{
   sge_dstring_append(dstr_mid, "#if defined(__SGE_GDI_LIBRARY_HOME_OBJECT_FILE__)\n\n");
   sge_dstring_sprintf_append(dstr_mid, "%s\n\n", EXTERN_C_BEGIN);
   sge_dstring_append(dstr_mid, "lNameSpace nmv[] = {\n\n");

   sge_dstring_append(dstr_mid, "/*\n" \
                                "   1. unique keq of the first element in the descriptor\n" \
                                "   2. number of elements in the descriptor\n" \
                                "   3. array with names describing the fields of the descriptor\n" \
                                "   4. pointer to the descriptor\n" \
                                "      1.              2.   3.   4.\n" \
                                "*/\n\n");


   return sge_dstring_get_string(dstr_mid);
}

static const char *
dump_all_lists_post(dstring *dstr_post)
{
   sge_dstring_append(dstr_post, "};\n\n");
   sge_dstring_sprintf_append(dstr_post, "%s\n\n", EXTERN_C_END);
   sge_dstring_append(dstr_post, "#else\n");
   sge_dstring_append(dstr_post, "#ifdef __SGE_GDI_LIBRARY_SUBLIST_FILE__\n");
   sge_dstring_append(dstr_post, "#else\n");
   sge_dstring_sprintf_append(dstr_post, "%s\n\n", EXTERN_C_BEGIN);
   sge_dstring_append(dstr_post, "extern lNameSpace nmv[];\n\n");
   sge_dstring_sprintf_append(dstr_post, "%s\n\n", EXTERN_C_END);
   sge_dstring_append(dstr_post, "#endif /* __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__ */\n\n");
   sge_dstring_append(dstr_post, "#endif /* __SGE_GDI_LIBRARY_SUBLIST_FILE__     */\n\n");

   return sge_dstring_get_string(dstr_post);
}

static bool
dump_all_lists(dstring *dstr_all_types_includes, dstring *dstr_all_types_body,
               const char *source_dir, const char *target_dir)
{
   bool ret = true;
   /* prerequisits, read in / is available
    *    - copyright text        - added in the beginning of every file
    *    - all_lists_pre         - docs
    *    - all_lists_mid         - after includes of *_L.h files
    *    - all_lists_post        - end of the file after dumping the namespace
    */
   dstring dstr_pre = DSTRING_INIT;
   dstring dstr_mid = DSTRING_INIT;
   dstring dstr_post = DSTRING_INIT;
   const char *str_copyright = read_file(source_dir, "copyright.txt");
   const char *str_all_lists_pre = dump_all_lists_pre(&dstr_pre);
   const char *str_all_lists_mid = dump_all_lists_mid(&dstr_mid);
   const char *str_all_lists_post = dump_all_lists_post(&dstr_post);
   const char *str_includes = sge_dstring_get_string(dstr_all_types_includes);
   const char *str_body = sge_dstring_get_string(dstr_all_types_body);

   if (str_copyright == NULL ||
       str_all_lists_pre == NULL || str_all_lists_mid == NULL || str_all_lists_post == NULL) {
      fprintf(stderr, "all_lists input files are not available\n");
      ret = false;
   }

   if (ret) {
      char filename[SGE_PATH_MAX];
      FILE *fp;
      sprintf(filename, "%s/sge_all_listsL.h", target_dir);
      fp = fopen(filename, "w");
      fputs("#ifndef __SGE_ALL_LISTSL_H\n#define __SGE_ALL_LISTSL_H\n", fp);
      fwrite(str_copyright, strlen(str_copyright), 1, fp);
      fwrite(str_all_lists_pre, strlen(str_all_lists_pre), 1, fp);
      if (str_includes != NULL) {
         fwrite(str_includes, strlen(str_includes), 1, fp);
      }
      fwrite(str_all_lists_mid, strlen(str_all_lists_mid), 1, fp);
      if (str_body != NULL) {
         fwrite(str_body, strlen(str_body), 1, fp);
      }
      fwrite(str_all_lists_post, strlen(str_all_lists_post), 1, fp);
      fputs("#endif /* __SGE_ALL_LISTSL_H */\n", fp);
      fclose(fp);
      //printf("wrote file %s\n", filename);
   }

   // cleanup
   sge_free(&str_copyright);
   sge_dstring_free(&dstr_pre);
   sge_dstring_free(&dstr_mid);
   sge_dstring_free(&dstr_post);

   return ret;
}


static void
dump_cull_boundaries_begin(dstring *dstr_boundaries)
{
   sge_dstring_append(dstr_boundaries, "#include \"cull/cull_list.h\"\n\n" \
                      "/*----------------------------------------------------\n" \
                      " *\n" \
                      " * 1. sge_boundaries.h\n" \
                      " *\n" \
                      " *    is included by every file of group 2\n" \
                      " *    defines boundaries of all lists\n" \
                      " *\n" \
                      " * 2. sge_jobL.h ...\n" \
                      " * \n" \
                      " *    is used to define a specific list like:\n" \
                      " *    - job list\n" \
                      " *    - ...\n" \
                      " *    \n" \
                      " *--------------------------------------------------\n" \
                      " */\n\n");
   sge_dstring_sprintf_append(dstr_boundaries, "%s\n\n", EXTERN_C_BEGIN);

   sge_dstring_append(dstr_boundaries, "/* BASIC_UNIT and MAX_DESCR_SIZE is defined in cull/cull_list.h */\n\n");
   sge_dstring_append(dstr_boundaries, "enum NameSpaceBoundaries {\n\n");
}

static void
dump_cull_boundaries_end(dstring *dstr_boundaries)
{
   sge_dstring_append(dstr_boundaries, "\n#  define LAST_UPPERBOUND PACK_UPPERBOUND\n\n");
   sge_dstring_append(dstr_boundaries, "};\n\n");
   sge_dstring_sprintf_append(dstr_boundaries, "%s\n\n", EXTERN_C_END);
}

static bool
dump_cull(const char *source_dir, const char *target_dir)
{
   bool ret = true;
   dstring dstr_boundaries = DSTRING_INIT;
   dstring dstr_all_types_includes = DSTRING_INIT;
   dstring dstr_all_types_body = DSTRING_INIT;

   dump_cull_boundaries_begin(&dstr_boundaries);

   cJSON *json_all = read_json_from_file(source_dir, "all_types.json");
   if (json_all != NULL) {
      cJSON *json_types = cJSON_GetObjectItem(json_all, "types");
      if (json_types != NULL && !cJSON_IsNull(json_types)) {
         int num_types = cJSON_GetArraySize(json_types);
         int i;
         const char *last_cull_prefix = NULL;
         for (i = 0; i < num_types /*&& i < 2*/; i++) {
            cJSON *json_type = cJSON_GetArrayItem(json_types, i);
            ret = dump_cull_type(json_type, &last_cull_prefix, &dstr_boundaries,
                                 &dstr_all_types_includes, &dstr_all_types_body,
                                 source_dir, target_dir);
         }
         // add PACK_Type (which is not in the namespace and therefore not in our json files)
         sge_dstring_sprintf_append(&dstr_boundaries, "   PACK_LOWERBOUND = %s_UPPERBOUND + 1,\n",
                                    last_cull_prefix);
         sge_dstring_sprintf_append(&dstr_boundaries, "   PACK_UPPERBOUND = PACK_LOWERBOUND + 1*BASIC_UNIT - 1\n");
         // add terminating NULL entry
         sge_dstring_sprintf_append(&dstr_all_types_body, "   {0, 0, NULL, NULL}\n");
      } else {
         fprintf(stderr, "types not found in all_types.json\n");
         ret = false;
      }
   }

   if (ret) {
      dump_cull_boundaries_end(&dstr_boundaries);
   }

   if (ret) {
      ret = dump_all_boundaries(&dstr_boundaries, source_dir, target_dir);
   }

   if (ret) {
      ret = dump_all_lists(&dstr_all_types_includes, &dstr_all_types_body, source_dir, target_dir);
   }

   // cleanup
   cJSON_Delete(json_all);
   sge_dstring_free(&dstr_boundaries);
   sge_dstring_free(&dstr_all_types_includes);
   sge_dstring_free(&dstr_all_types_body);

   return ret;
}

int main(int argc, char *argv[])
{
   bool ret = true;
   const char *source_dir = NULL;
   const char *target_language = NULL;
   const char *target_dir = NULL;

   if (argc < 3) {
      fprintf(stderr, "usage: %s <source_dir> <target_language> <target_dir>\n", argv[0]);
      fprintf(stderr, "   target_language is CULL, (not yet implemented: C++, Go, Java)\n");
      ret = false;
   }

   if (ret) {
      source_dir = argv[1];
      target_language = argv[2];
      target_dir = argv[3];
      printf("reading json files from %s, writing %s to %s\n", source_dir, target_language, target_dir);
   }

   if (ret) {
      if (strcmp(target_language, "CULL") == 0) {
         ret = dump_cull(source_dir, target_dir);
#if 0
      } else if (strcmp(target_language, "C++") == 0) {
      } else if (strcmp(target_language, "Go") == 0) {
      } else if (strcmp(target_language, "Java") == 0) {
#endif
      } else {
         fprintf(stderr, "target_language %s not implemented\n", target_language);
         ret = false;
      }
   }

   return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}
