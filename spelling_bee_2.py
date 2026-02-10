#!/usr/bin/env python

import os
import sys
import argparse
import re
import itertools
from more_itertools import distinct_permutations

THIS = os.path.basename(__file__)
THIS_VERSION = "0.1"

#DEFAULT_WORD_LIST_PATH = '/usr/share/dict/words'
#DEFAULT_WORD_LIST_PATH = '/home/smacdonald/Downloads/english.txt'
#DEFAULT_WORD_LIST_PATH = '/home/smacdonald/Downloads/sowpods.txt'
DEFAULT_WORD_LIST_PATH = '/home/stuart/personal/word_lists/sowpods2.txt'
MIN_WORD_LENGTH = 4
MAX_WORD_LENGTH = 11

def word_list(path):
    with open(path, 'r') as f:
        while True:
            word = f.readline().strip()
            if not word:
                break
            if len(word) < MIN_WORD_LENGTH:
                continue
            if re.search('[^\w]', word):
                continue
            yield word.lower()

#def load_word_list(path):
#    with open(path, 'r') as file:
#        return set(re.sub('[^\w]', ' ', file.read()).split())

arg_parser = argparse.ArgumentParser(prog=THIS, description='Spelling Bee solver')
arg_parser.add_argument('letters', type=str, help='Spelling Bee letter set, required letter first, must not have repeats')
args = arg_parser.parse_args()

#letters = set(args.letters)
#n = len(letters)
#if n != 7:
#    print(f'{THIS}: Error: 7 letters expected, {n} letter{"s"[:n^1]} provided \'{args.letters}\'')
#    sys.exit(1)

#words = load_word_list(DEFAULT_WORD_LIST_PATH)

required_letter = args.letters[0].lower()
letters = set(args.letters.lower())

word_count = 0
for word in word_list(DEFAULT_WORD_LIST_PATH):
    word_letters = set(word)
    if required_letter not in word_letters:
        continue
    if not word_letters.issubset(letters):
        continue
    word_count += 1
    if len(word_letters) == 7:
        print(word, 'PANAGRAM')
    else:
        print(word)
print(f'Found {word_count} words')

#for word_len in range(MIN_WORD_LENGTH, MAX_WORD_LENGTH + 1):
#    print(f'-= {word_len}')
#    for word in itertools.product(letters, repeat=word_len):
#        word = ''.join(word)
#        #print(word)
#        if required_letter not in word:
#            #print(word, 'SKIPPED')
#            continue
#        if word in words:
#            if len(set(word)) == 7:
#                print(word, 'PANAGRAM')
#            else:
#                print(word)

