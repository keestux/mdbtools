/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000 Brian Bruns
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**************************************************************
	This utility allows you to list the queries in the database
	and dump the SQL associated with a specific query.  
	
	The	idea behind this utility is to eventually allow the 
	results to be piped to mdb-sql allowing a (fairly) simple 
	method to execute queries that are stored in the access
	database.
	
	created by Leonard Leblanc <lleblanc@macroelite.ca>
	(with much help from the other mdb utilities)

	Modified by Evan Miller and added to repository 2020
**************************************************************/
	
#include "mdbtools.h"

#undef MDB_BIND_SIZE
#define MDB_BIND_SIZE 200000

void mdb_list_queries(MdbHandle *mdb, int line_break, char *delimiter);
char * mdb_get_query_id(MdbHandle *mdb,char *query);

int main (int argc, char **argv) {
	unsigned int i;
	MdbHandle *mdb;
	MdbCatalogEntry *entry,*sys_queries,*temp;
	MdbTableDef *table;
	char *delimiter = NULL;
	int list_only=0;
	int found_match=0;
	int line_break=0;
	int opt;	
	char *query_id;
	
	// variables for the msysqueries table. hopefully 256 is big enough
	char *attribute = (char *) malloc(MDB_BIND_SIZE);
	char *expression = (char *) malloc(MDB_BIND_SIZE);
	char *flag = (char *) malloc(MDB_BIND_SIZE);
	char *name1 = (char *) malloc(MDB_BIND_SIZE);
	char *name2 = (char *) malloc(MDB_BIND_SIZE);
	char *objectid = (char *) malloc(MDB_BIND_SIZE);
	char *order = (char *) malloc(MDB_BIND_SIZE);
	
	//variables for the generation of sql
	char *sql_tables = (char *) malloc(MDB_BIND_SIZE);
	char *sql_columns = (char *) malloc(MDB_BIND_SIZE);
	char *sql_where = (char *) malloc(MDB_BIND_SIZE);
	char *sql_sorting = (char *) malloc(MDB_BIND_SIZE);
	
	/* see getopt(3) for more information on getopt and this will become clear */
	while ((opt=getopt(argc, argv, "L1d:"))!=-1) {
        switch (opt) {
			case 'L':
				list_only = 1;
				break;
			case '1':
				line_break = 1;
				break;
			case 'd':
            	delimiter = (char *) malloc(strlen(optarg)+1);
				strcpy(delimiter, optarg);
				break;
		}
	}

	/* we've parsed all the options and we should at least have a database name left */
	if ((argc - optind) < 1) {
		fprintf (stderr, "Usage: %s [options] <database filename> <query name>\n",argv[0]);
		fprintf (stderr, "where options are:\n");
		fprintf (stderr, "  -L\t\t\tList queries in the database (default if no query name is passed)\n");
		fprintf (stderr, "  -1\t\t\tUse newline as the delimiter (used in conjuction with listing)\n");
		fprintf (stderr, "  -d <delimiter>\tSpecify delimiter to use\n");
		exit (1);
	}
	
	/* let's turn list_only on if only a database filename was passed */
	if((argc-optind) < 2)
		list_only=1;
	
 	/* open the database */
 	if (!(mdb = mdb_open(argv[optind],MDB_NOFLAGS))) {
		fprintf(stderr,"Couldn't open database.\n");
		exit(1);
	}

 	/* read the catalog */
 	if (!mdb_read_catalog (mdb, MDB_ANY)) {
		fprintf(stderr,"File does not appear to be an Access database\n");
		exit(1);
	}
	
	if(list_only) { 
		mdb_list_queries(mdb,line_break,delimiter);
	} else {
		/* let's get the entry for the user specified query
			 we also want to get the catalog for the MSysQueries table
			 while we are here */
		for (i=0; i < mdb->num_catalog; i++) {
			temp = g_ptr_array_index(mdb->catalog, i);
			
			if(strcmp(temp->object_name,argv[optind+1]) == 0) {
				entry = g_ptr_array_index(mdb->catalog,i);
				found_match=1;
			} else if(strcmp(temp->object_name,"MSysQueries") == 0) {
				sys_queries = g_ptr_array_index(mdb->catalog,i);
			} 
		}
		
		if(found_match) {
			/* Let's get the id for the query */
			query_id = mdb_get_query_id(mdb,entry->object_name);
			
			table = mdb_read_table(sys_queries);

			if(table) {
				mdb_read_columns(table);

				mdb_bind_column_by_name(table, "Attribute", attribute, NULL);
				mdb_bind_column_by_name(table, "Expression", expression, NULL);
				mdb_bind_column_by_name(table, "Flag", flag, NULL);
				mdb_bind_column_by_name(table, "Name1", name1, NULL);
				mdb_bind_column_by_name(table, "Name2", name2, NULL);
				mdb_bind_column_by_name(table, "ObjectId", objectid, NULL);
				mdb_bind_column_by_name(table, "Order", order, NULL);

				mdb_rewind_table(table);

				while (mdb_fetch_row(table)) {
					if(strcmp(query_id,objectid) == 0) {
						//we have a row for our query
						switch(atoi(attribute)) {
							case 5:		// table name
								if(strcmp(sql_tables,"") == 0) {
									strcpy(sql_tables,name1);
								} else {
									strcat(sql_tables,",");
									strcat(sql_tables,name1);
								}
								break;
							case 6:		// column name
								if(strcmp(sql_columns,"") == 0) {
									strcpy(sql_columns,expression);
								} else {
									strcat(sql_columns,",");
									strcat(sql_columns,expression);
								}
								break;
							case 7:		// join/relationship where clause
								//fprintf(stdout,"join tables: %s - %s\n",name1,name2);
								//fprintf(stdout,"join clause: %s\n",expression);
								break;
							case 8:		// where clause
								strcpy(sql_where,expression);
								break;
							case 11:		// sorting
								if(strcmp(sql_sorting,"") == 0) {
									strcpy(sql_sorting,"ORDER BY ");
									strcat(sql_sorting,expression);
									if(strcmp(name1,"D") == 0) {
										strcat(sql_sorting," DESCENDING");
									}
								}
								break;
						}
					}
				}
				
				/*fprintf(stdout,"sql_tables: %s\n",sql_tables);
				fprintf(stdout,"sql_columns: %s\n",sql_columns);
				fprintf(stdout,"sql_where: %s\n",sql_where);
				fprintf(stdout,"sql_sorting: %s\n",sql_sorting);*/
				
				/* print out the sql statement */
				if(strcmp(sql_where,"") == 0) {
					fprintf(stdout,"SELECT %s FROM %s %s\n",sql_columns,sql_tables,sql_sorting);
				} else {
					fprintf(stdout,"SELECT %s FROM %s WHERE %s %s\n",sql_columns,sql_tables,sql_where,sql_sorting);
				}
						
				mdb_free_tabledef(table);
			}
		} else {
			fprintf(stderr,"Couldn't locate the specified query: %s\n",argv[optind+1]);
		}
	}

	mdb_close(mdb);
	
	if(delimiter) free(delimiter);

	exit(0);
}

/**************************************************** 	
		mdb_list_queries
		Description: 	This function prints the list of queries to stdout
		Parameters:	MdbHandle *mdb (handle for the current database)
								int line_break (whether or not to place a line break between each query)
								char *delimiter (delimiter when not using a line break)
		Returns:			nothing
****************************************************/
void mdb_list_queries(MdbHandle *mdb, int line_break, char *delimiter) {
	unsigned int i;
	MdbCatalogEntry *entry;
	
 	/* loop over each entry in the catalog */
 	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index(mdb->catalog, i);

     	/* if it's a query */
     	if (entry->object_type == MDB_QUERY) {
       		if (line_break) 
				fprintf (stdout, "%s\n", entry->object_name);
			else if (delimiter) 
				fprintf (stdout, "%s%s", entry->object_name, delimiter);
			else 
				fprintf (stdout, "%s ", entry->object_name);
		}
	}
	if (!line_break) 
		fprintf (stdout, "\n");
}

/**************************************************** 	
		mdb_get_query_id
		Description: 	This function returns the id of the passed query
		Parameters:	MdbHandle *mdb (handle for the current database)
								char *query (name of the desired query to retrieve)
		Returns:			char * (id of the current query)
****************************************************/
char * mdb_get_query_id(MdbHandle *mdb, char *query) {
	unsigned int i;
	MdbCatalogEntry *entry;
	MdbTableDef *table;
	static char id[256];
	char name[256];
	
 	/* loop over each entry in the catalog */
 	for (i=0; i < mdb->num_catalog; i++) {
		entry = g_ptr_array_index(mdb->catalog, i);

		if(strcmp(entry->object_name,"MSysObjects") == 0) {
			break;
		}
	}
	
	table = mdb_read_table(entry);

	if(table) {
		mdb_read_columns(table);

		mdb_bind_column_by_name(table, "Id", id, NULL);
		mdb_bind_column_by_name(table, "Name", name, NULL);

		mdb_rewind_table(table);

		while (mdb_fetch_row(table)) {
			if(strcmp(query,name) == 0) {
				break;
			}
		}
		mdb_free_tabledef(table);
	}
	
	return(id);
}
