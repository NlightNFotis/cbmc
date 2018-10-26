#!/usr/bin/env python
"""Script to filter out old warnings from doxygen."""

import sys
import re


def compare_lists(list1, list2):
    """Simple comparison of two lists."""
    if len(list1) != len(list2):
        return False
    for (elt1, elt2) in zip(list1, list2):
        if elt1 != elt2:
            return False
    return True


class DoxygenWarning(object):
    """Doxygen warning class."""

    def __init__(self, firstline, filename, warning):
        self.firstline = firstline
        self.filename = filename
        self.warning = warning
        self.otherlines = []

    def approx_equals(self, other):
        """Return true if warnings have same filename and warning message."""
        if self.filename != other.filename:
            return False
        if self.warning != other.warning:
            return False
        return compare_lists(self.otherlines, other.otherlines)

    def is_unexpected(self, expected_warnings):
        """Return true unless this is in the list of expected warnings."""
        for expected in expected_warnings:
            if self.approx_equals(expected):
                return False
        return True


def build_warnings_list(all_lines):
    """Create a list of the warnings in this file."""
    warnings_list = []
    current = None

    warning_start_expr = re.compile(r'[^:]+/([^:/]+):\d+: (warning.*$)')

    for line in all_lines:
        if line.isspace():
            continue

        # Allow comments in the list of expected warnings.
        if line.startswith('#'):
            continue

        matched = warning_start_expr.match(line)
        if matched:
            filename = matched.group(1)
            warning = matched.group(2)
            current = DoxygenWarning(line.strip(), filename, warning)
            warnings_list.append(current)
        elif line.startswith('  '):
            current.otherlines.append(line.strip())
        else:
            # Assuming all warnings are of the form [path:line: warning:...]
            # (and the warnings about too many nodes have been filtered out).
            print('Error filtering warnings: Unexpected input format.')
            print('  Input:' + line)

    return warnings_list


def ignore_too_many_nodes(all_lines):
    """Filter out lines about graphs with too many nodes."""
    too_many_nodes_expr = re.compile(
        r'warning: Include(d by)? graph for .* not generated, too many nodes. '
        + r'Consider increasing DOT_GRAPH_MAX_NODES.')
    return [x for x in all_lines if not too_many_nodes_expr.match(x)]


def filter_expected_warnings(expected_warnings_path):
    """Filter lines from stdin and print to stdout."""
    with open(expected_warnings_path, "r") as warnings_file:
        expected_warnings = build_warnings_list(warnings_file.readlines())

    new_warnings = build_warnings_list(
        ignore_too_many_nodes(sys.stdin.readlines()))

    for warning in new_warnings:
        if warning.is_unexpected(expected_warnings):
            print(warning.firstline)
            for line in warning.otherlines:
                print('  ' + line)


if __name__ == "__main__":

    if len(sys.argv) != 2:
        print('usage: filter_expected_warnings.py <expected_warnings_file>')
        print('(warnings from stdin are filtered and printed to stdout)')
        sys.exit()

    filter_expected_warnings(sys.argv[1])
