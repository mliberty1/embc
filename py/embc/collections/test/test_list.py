# Copyright (c) 2017 Jetperch LLC.  All rights reserved.

import unittest
import embc


class TestList(unittest.TestCase):

    def test_construct(self):
        i = embc.collections.list.new()

