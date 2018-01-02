# Copyright 2017 Jetperch LLC

"""EMBC error codes from embc/ec.h"""

name_to_num = {
    'SUCCESS': 0,
    'UNSPECIFIED': 1,
    'NOT_ENOUGH_MEMORY': 2,
    'NOT_SUPPORTED': 3,
    'IO': 4,
    'PARAMETER_INVALID': 5,
    'INVALID_RETURN_CONDITION': 6,
    'INVALID_CONTEXT': 7,
    'INVALID_MESSAGE_LENGTH': 8,
    'MESSAGE_INTEGRITY': 9,
    'SYNTAX_ERROR': 10,
    'TIMED_OUT': 11,
    'FULL': 12,
    'EMPTY': 13,
    'TOO_SMALL': 14,
    'TOO_BIG': 15,
    'NOT_FOUND': 16,
    'ALREADY_EXISTS': 17,
    'PERMISSIONS': 18,
    'BUSY': 19,
    'UNAVAILABLE': 20,
    'IN_USE': 21,
    'CLOSED': 22,
    'SEQUENCE': 23,
    'ABORTED': 24,
    'SYNCHRONIZATION': 25,
}

num_to_name = {}
for name, num in name_to_num.items():
    num_to_name[num] = name
    globals()[name] = num
