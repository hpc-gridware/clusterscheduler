---
title: sge_category
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_category - xxQS_NAMExx category format

# DESCRIPTION

Categories are automatically managed objects in xxQS_NAMExx. They group jobs that have the same scheduling characteristics. Categories are created by the system and are also automatically adjusted as required.

Each job in the system belongs to exactly one category, but a category can belong to several jobs. When the last job of a category leaves the system, the category is deleted.

Categories are identified by a unique ID, and have a string appended to them, usually reflecting the submit switches used either when the job was submitted or when the job was modified. `qconf -scatl` will show a list of all the categories in the system. The list will be sorted by category ID. It will also show the number of jobs in the category and the category string. `qstat -j <job_id>` will show the category ID of a job.

Categories are used in the Job Scheduler to group jobs which have the same scheduling characteristics. If a job of a particular category cannot be scheduled, then also no other job of that category can be started. Categories allow the Job Scheduler to find an early cut-off point in the scheduling algorithm to reduce the scheduling overhead.

Job Submission Verifiers can be used to reduce the number of categories in the system by adjusting the submit switches of submitted jobs. To achieve this, the characteristics of these submitted jobs can be made consistent. For example, storage requests can be rounded up to the nearest multiple of 100MB. This reduces the number of categories in the system and therefore the scheduling overhead.

# FORMAT 

## id 

Unique category ID. The ID is assigned by the system and identifies the category as long as it exists. Category IDs of deleted categories may be reused later for other new categories.

## rcount

Reference count. The number of jobs that are currently assigned to this category. The reference count is incremented when a job is assigned to the category and decremented when the job leaves the system or is modified to belongs to a different category.

## str 

The category string. The string is assigned by the system and reflects the submit switches or other characteristics of a job that were present either when the job was submitted or when the job was modified. At any point in time one category string is unique in the system.

Please note that the category strings may be different for different versions of xxQS_NAMExx. They also depend on other configuration parameters such as Resource Quota Sets, Access Lists or Projects.

# SEE ALSO

xxqs_name_sxx_intro(1), qconf(1), qsub(1), xxqs_name_sxx_jsv(1), xxqs_name_sxx_qmaster(8)  

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
