#pragma once
/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/       

/** @file
 * @brief Rendering cull objects as text, and parsing them back
 */


/** @defgroup spool_flatfile Flatfile spooling and qconf formatting
 * @brief Rendering cull objects as text, and parsing them back
 *
 * Two things use this module, and that is why it is bigger than a spooling
 * backend needs to be: the classic spooling backend writes the master lists
 * into the spool directory with it, and **`qconf` formats every `-s*` output
 * with it**. The same writer produces the spool file and the text a user
 * edits under `qconf -mq`.
 *
 * The shape of the output comes from a #spool_flatfile_instr. Three
 * delimiters do the work, and they are easy to confuse:
 *
 * | delimiter | goes between |
 * |---|---|
 * | `name_value_delimiter` | a field's name and its value, if names are shown |
 * | `field_delimiter` | the fields of one record |
 * | `record_delimiter` | the records, i.e. the elements of a list |
 *
 * Which attributes appear at all is not decided here - that is the
 * #spool_instr_t from @ref spool_utilities, turned into a #spooling_field array.
 * This module only decides how they are laid out.
 *
 * @see @ref spool_utilities
 * @{
 */

#include "cull/cull.h"

#include "spool/sge_spooling_utilities.h"

/** @brief Width the aligned output pads a line to */
#define MAX_LINE_LENGTH 80

/** @brief Where a write function shall put its output */
typedef enum {
   SP_DEST_STDOUT,   ///< Standard output, for `qconf -s*`
   SP_DEST_STDERR,   ///< Standard error
   SP_DEST_TMP,      ///< A temporary file, whose name is returned - this is what `qconf -m*` hands to the editor
   SP_DEST_SPOOL     ///< The named file, for the classic spooling backend
} spool_flatfile_destination;

/** @brief The text format to read or write */
typedef enum {
   SP_FORM_ASCII,    ///< The traditional `name value` layout, shaped by the #spool_flatfile_instr
   SP_FORM_XML,      ///< XML
   SP_FORM_CULL,     ///< The cull representation
   SP_FORM_JSON      ///< Structured JSON, `qconf -fmt json` (CS-2313a)
} spool_flatfile_format;


/** @brief How to render a self-nesting object, i.e. the share tree
 *
 * A share tree node holds its children in one of its own fields, so writing
 * it out means recursing. All three members are `NoName` for the flat
 * objects, which is every object but that one.
 */
typedef struct recursion_info {
   int recursion_field;   ///< The field holding the child elements
   int id_field;          ///< The field identifying an element, so the tree can be rebuilt on read
   int supress_field;     ///< A field printed for the root element only
} recursion_info_t;

/** @brief The layout of one level of text output
 *
 * A `'\0'` in any of the five delimiters means "emit nothing here". The
 * reader accepts any amount of whitespace between tokens regardless of what
 * the writer put there.
 *
 * @todo (JG) There is no check function. Nothing stops a delimiter from being
 *       whitespace other than `\n`, which the reader could not tell from
 *       padding.
 */
typedef struct spool_flatfile_instr {
   const spool_instr_t *spool_instr;   ///< Which attributes to spool; nullptr when the caller passes a #spooling_field array directly
   bool show_field_names;              ///< Write each field's name before its value
   bool show_field_header;             ///< Write one header line naming the columns, for tabular output
   bool show_footer;                   ///< Write a closing line after the records
   bool align_names;                   ///< Pad the names so the values line up in a column
   bool align_data;                    ///< Pad the values so the columns line up
   bool record_start_end_newline;      ///< Put #record_start and #record_end on lines of their own and indent the body - the braced style of `qconf -srqs`
   bool show_empty_fields;             ///< Write a field even when it has no value
   bool ignore_list_name;              ///< Do not prefix the records with the name of the list they came from
   const char name_value_delimiter;    ///< Between a field's name and its value
   const char field_delimiter;         ///< Between the fields of one record
   const char record_delimiter;        ///< Between records, i.e. between the elements of a list
   const char record_start;            ///< Written before each record
   const char record_end;              ///< Written after each record
   const struct spool_flatfile_instr *sub_instr;   ///< The layout for sublist elements; several of these point at themselves, which is how nesting to any depth reuses one layout
   const recursion_info_t recursion_info;   ///< Set only for the share tree, see #recursion_info
} spool_flatfile_instr;

/** @brief What to spool and how, as one argument
 *
 * Pairs the field selection with the layout so that a caller can keep both
 * for an object type in a single table.
 */
typedef struct flatfile_info {
   spooling_field *fields;             ///< The attributes to write, terminated by a `NoName` entry
   const spool_flatfile_instr *instr;  ///< The layout to write them in
} flatfile_info;

/** @name The ready made qconf layouts
 *
 * One #spool_flatfile_instr per output shape `qconf` and the classic backend
 * need. The `_sub_` ones are never used at top level; they are what a
 * top level layout names as its #spool_flatfile_instr::sub_instr for the
 * elements of a sublist.
 *
 * The examples below show the delimiters, not real attribute names.
 * @{
 */
extern const spool_flatfile_instr qconf_sub_name_value_space_sfi;   ///< Sublist as `a=b c=d` - and its own `sub_instr`, so it nests to any depth
extern const spool_flatfile_instr qconf_sfi;                        ///< **The** `qconf -s*` layout: one `name value` per line, values aligned into a column, sublists space separated
extern const spool_flatfile_instr qconf_sub_comma_list_sfi;         ///< Sublist as one record of comma separated fields, `a,b,c`
extern const spool_flatfile_instr qconf_name_value_list_sfi;        ///< `name=value` per line, records comma separated
extern const spool_flatfile_instr qconf_sub_name_value_comma_sfi;   ///< Sublist as `a=b,c=d`
extern const spool_flatfile_instr qconf_sub_sub_name_value_comma_sfi;   ///< Second level sublist as `{a%b:c%d}`
extern const spool_flatfile_instr qconf_sub_name_list_sfi;          ///< Sublist as `a%b:c%d`, unbraced
extern const spool_flatfile_instr qconf_sub_comma_sfi;              ///< Sublist as comma separated records of one field each
extern const spool_flatfile_instr qconf_param_sfi;                  ///< Parameter lists: `name value` per line, sublists comma separated
extern const spool_flatfile_instr qconf_sub_param_sfi;              ///< Sublist of a parameter list, space separated with the data aligned
extern const spool_flatfile_instr qconf_cat_sfi;                    ///< Category output - field for field the same as #qconf_sfi
extern const spool_flatfile_instr qconf_cat_list_sfi;               ///< Tabular category list: a header line, then one aligned record per line
extern const spool_flatfile_instr qconf_comma_sfi;                  ///< As #qconf_sfi, but sublists comma separated instead of space separated
extern const spool_flatfile_instr qconf_ce_sfi;                     ///< Complex entry output - field for field the same as #qconf_sfi
extern const spool_flatfile_instr qconf_ce_list_sfi;                ///< The `qconf -sc` table: header line, aligned columns, footer
extern const spool_flatfile_instr qconf_sub_rqs_sfi;                ///< One resource quota rule per line, names shown, sublists comma separated
extern const spool_flatfile_instr qconf_sub_spool_usage_sfi;        ///< Usage records terminated by `;`
extern const spool_flatfile_instr qconf_rqs_sfi;                    ///< The braced `qconf -srqs` layout: `{` and `}` on their own lines with the body indented
extern const spool_flatfile_instr qconf_sub_name_value_comma_braced_sfi;   ///< Sublist as `[a=b,c=d]`
/** @} */

/** @cond doxygen_note
 * #qconf_sfi, #qconf_ce_sfi and #qconf_cat_sfi are byte for byte identical -
 * three names for one layout, kept apart so that one of them could be changed
 * without touching the others. None of them ever has been.
 * @endcond
 */


const char *
spool_flatfile_write_object(lList **answer_list, const lListElem *object,
                            bool is_root, const spooling_field *fields,
                            const spool_flatfile_instr *instr,
                            const spool_flatfile_destination destination,
                            const spool_flatfile_format format,
                            const char *filepath, bool print_header,
                            const char *json_type_name = nullptr);

const char *
spool_flatfile_write_list(lList **answer_list,
                          const lList *list,
                          const spooling_field *fields,
                          const spool_flatfile_instr *instr,
                          const spool_flatfile_destination destination,
                          const spool_flatfile_format format,
                          const char *filepath, bool print_header);

lListElem *
spool_flatfile_read_object(lList **answer_list, const lDescr *descr, lListElem *root,
                           const spooling_field *fields_in, int fields_out[],
                           bool parse_values, const spool_flatfile_instr *instr,
                           const spool_flatfile_format format,
                           FILE *file,
                           const char *filepath);
lList *
spool_flatfile_read_list(lList **answer_list, const lDescr *descr, 
                         const spooling_field *fields_in, int fields_out[],
                         bool parse_values, const spool_flatfile_instr *instr,
                         const spool_flatfile_format format,
                         FILE *file,
                         const char *filepath);

bool 
spool_flatfile_align_object(lList **answer_list,
                            spooling_field *fields);

bool
spool_flatfile_align_list(lList **answer_list, const lList *list, 
                          spooling_field *fields, int padding);

int spool_get_unprocessed_field(spooling_field in[], int out[], lList **alpp);
int spool_get_number_of_fields(const spooling_field fields[]);

/** @} */
