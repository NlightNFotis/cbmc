#!/usr/bin/env python3

import os
from pathlib import Path

TEST_LEVELS = ['CORE', 'KNOWNBUG', 'THOROUGH']

class InvalidTestDescException(Exception):
    pass

class Stats(object):
    def __init__(self):
        self.total_tests = 0
        self.directories = 0
        self.core_tests = 0
        self.knownbug_tests = 0
        self.thorough_tests = 0

    def print_stats(self):
        print('[STATS] Total tests: ', self.total_tests)
        print('[STATS] Tests marked CORE: ', self.core_tests)
        print('[STATS] Tests marked KNOWNBUG: ', self.knownbug_tests)
        print('[STATS] Tests marked THOROUGH: ', self.thorough_tests)

class TestRunner(object):
    def __init__(self):
        self.stats = Stats()
        self.root = os.getcwd()

    def run_tests(self):
        top_level_folders = self.enumerate_folders(self.root)
        for folder in top_level_folders:
            tests = self.enumerate_folders(folder)
            self.stats.total_tests += len(tests)
        # epilogue of test running: print test statistics
        self.print_statistics()

    def enumerate_folders(self, start_path):
        path = Path(start_path)
        return [d for d in path.iterdir() if d.is_dir()]
    
    def print_statistics(self):
        self.stats.print_stats()


class DescParser(object):
    def __init__(self):
        pass

    def parse_desc(self, file):
        ''' Start parsing a .desc file. At the end of execution of this function
            we will return a valid TestDescription object, that will drive the
            testing execution to follow later.
        '''
        desc_file = open(file, 'r')
        level, tags = parse_level_and_tags(desc_file.readline())
        # TODO: continue here

        
    def parse_level_and_tags(self, tag_string):
        '''Returns a tuple with the first element being the test level, and the
           rest being exclusion tags. E.g. ('CORE', ['winbug', 'macosbug'])
        '''
        tags = tag_string.split()
        if tags[0] not in TEST_LEVELS:
            raise InvalidTestDescException(
                'Test Level not within expected parameters. Got: ' + tags[0] +
                'expected any of: ' + TEST_LEVELS)
        return (tags[0], tags[1:])


class TestDescription(object):
    def __init__(self, level, tags):
        self.level = level
        self.tags = tags


# XXX: useful for debugging
def list_files(startpath):
    for root, dirs, files in os.walk(startpath):
        level = root.replace(startpath, '').count(os.sep)
        indent = ' ' * 4 * (level)
        print('{}{}/'.format(indent, os.path.basename(root)))
        subident = ' ' * 4 * (level + 1)
        for f in files:
            print('{}{}'.format(subident, f))

if __name__ == '__main__':
    test_runner = TestRunner()
    test_runner.run_tests()
