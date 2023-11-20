#!/usr/bin/env python

import os
import sys
import argparse
import re
import itertools
from more_itertools import distinct_permutations

THIS = os.path.basename(__file__)
THIS_VERSION = "0.1"

DEFAULT_WORD_LIST_PATH = '/usr/share/dict/american-english'
MIN_WORD_LENGTH = 4
MAX_WORD_LENGTH = 15

def load_word_list(path):
    with open(path, 'r') as file:
        return set(re.sub('[^\w]', ' ', file.read()).split())

def spelling_bee_generator(letters, min_length=MIN_WORD_LENGTH, max_length=MAX_WORD_LENGTH):
    def _spelling_bee_generator(letters, min_length, max_length):
        seen = set()
        orig_letters = ''.join(letters)
        letters = letters.lower()
        letters = [letters[0]] + sorted(letters[1:])
        print(f'-= spelling_bee_generator(letters = {orig_letters} {"".join(letters)}, min = {min_length}, max = {max_length})')
        if min_length == 1:
            yield letters[0]
            min_length += 1
        while min_length <= max_length:
            print(f'-= {min_length} < {max_length}')
            #for combo in itertools.product(letters, repeat=min_length):
            # Generate upper (transposed lower) triangular combos
            # https://stackoverflow.com/a/56962698
            for combo in itertools.combinations_with_replacement(letters, min_length):
                #print(f'-= it.product() = {combo}')
                if combo[0] != letters[0]:
                    #print('-= BREAKING')
                    break
                #if combo[-1] < combo[-2]:
                    #print("-= CONTINUING")
                for perm in distinct_permutations(combo):
                    word = ''.join(perm)
                    if word in seen:
                        seen_str = ', !!!!! SEEN !!!!!'
                        print(f'-= distinct_permutations() = {perm}{seen_str}')
                    else:
                        seen.add(word)
                        seen_str = ''
                    #print(f'-= distinct_permutations() = {perm}{seen_str}')
                    yield ''.join(perm)
            min_length += 1
    return _spelling_bee_generator(letters, min_length, max_length)

arg_parser = argparse.ArgumentParser(prog=THIS, description='Spelling Bee solver')
arg_parser.add_argument('letters', type=str, help='Spelling Bee letter set, required letter first, must not have repeats')
args = arg_parser.parse_args()

#letters = set(args.letters)
#n = len(letters)
#if n != 7:
#    print(f'{THIS}: Error: 7 letters expected, {n} letter{"s"[:n^1]} provided \'{args.letters}\'')
#    sys.exit(1)

words = load_word_list('/usr/share/dict/american-english')

#for word in spelling_bee_generator(args.letters, min_length=1, max_length=3):
#for word in spelling_bee_generator(args.letters, min_length=1, max_length=8):
word_count = 0
for word in spelling_bee_generator(args.letters):
    if word in words:
        word_count += 1
        if len(set(word)) == 7:
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

