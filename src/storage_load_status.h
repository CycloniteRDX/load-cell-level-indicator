#ifndef STORAGE_LOAD_STATUS_H
#define STORAGE_LOAD_STATUS_H


/*
 * Result of loading one validated persistent record.
 *
 * ABSENT includes both erased storage and a record that
 * was deliberately cleared through its public API.
 * INVALID means bytes were read successfully but did not
 * form a valid record. ACCESS_ERROR means the storage
 * operation itself could not be completed safely.
 */
typedef enum
{
    STORAGE_LOAD_VALID,
    STORAGE_LOAD_ABSENT,
    STORAGE_LOAD_INVALID,
    STORAGE_LOAD_ACCESS_ERROR
} storage_load_status_t;


#endif
