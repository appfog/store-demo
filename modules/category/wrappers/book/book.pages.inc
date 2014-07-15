<?php
// $Id: book.pages.inc,v 1.1 2008/07/11 05:45:09 jaza Exp $

/**
 * @file
 * User page callbacks for the book module.
 */

/**
 * Menu callback; prints a listing of all books.
 */
function book_render() {
  $book_list = array();
  foreach (book_get_books() as $book) {
    $book_list[] = l($book['title'], $book['href'], $book['options']);
  }

  return theme('item_list', $book_list);
}
