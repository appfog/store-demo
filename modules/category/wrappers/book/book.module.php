<?php
// $Id: book.module.php,v 1.1 2009/03/05 22:23:55 jaza Exp $

/**
 * @file
 * Wrapper module to provide a compatibility layer between the book
 * module and the category module.
 */

/**
 * Implementation of hook_theme()
 */
function book_theme() {
  return array(
    'book_title_link' => array(
      'arguments' => array('link' => NULL),
    ),
    'book_all_books_block' => array(
      'arguments' => array('book_menus' => array()),
      'template' => 'book-all-books-block',
    ),
  );
}

/**
 * Implementation of hook_menu().
 */
function book_menu() {
  $items['book'] = array(
    'title' => 'Books',
    'page callback' => 'book_render',
    'access arguments' => array('access content'),
    'type' => MENU_SUGGESTED_ITEM,
    'file' => 'book.pages.inc',
  );

  return $items;
}

/**
 * Implementation of hook_init(). Add's the book module's CSS.
 */
function book_init() {
  drupal_add_css(drupal_get_path('module', 'book') .'/book.css');
}

/**
 * Implementation of hook_block().
 *
 * Displays the book table of contents in a block when the current page is a
 * single-node view of a book node.
 */
function book_block($op = 'list', $delta = 0, $edit = array()) {
  $block = array();
  switch ($op) {
    case 'list':
      $block[0]['info'] = t('Book navigation');
      $block[0]['cache'] = BLOCK_CACHE_PER_PAGE | BLOCK_CACHE_PER_ROLE;
      return $block;
    case 'view':
      $current_bid = 0;
      if ($node = menu_get_object()) {
        $current_bid = empty($node->book['bid']) ? 0 : $node->book['bid'];
      }
      if (variable_get('book_block_mode', 'all pages') == 'all pages') {
        $block['subject'] = t('Book navigation');
        $book_menus = array();
        $pseudo_tree = array(0 => array('below' => FALSE));
        foreach (book_get_books() as $book_id => $book) {
          if ($book['bid'] == $current_bid) {
            // If the current page is a node associated with a book, the menu
            // needs to be retrieved.
            $book_menus[$book_id] = menu_tree_output(menu_tree_page_data($node->book['menu_name']));
          }
          else {
            // Since we know we will only display a link to the top node, there
            // is no reason to run an additional menu tree query for each book.
            $book['in_active_trail'] = FALSE;
            $pseudo_tree[0]['link'] = $book;
            $book_menus[$book_id] = menu_tree_output($pseudo_tree);
          }
        }
        $block['content'] = theme('book_all_books_block', $book_menus);
      }
      elseif ($current_bid) {
        // Only display this block when the user is browsing a book.
        $title = db_result(db_query(db_rewrite_sql('SELECT n.title FROM {node} n WHERE n.nid = %d'), $node->book['bid']));
        // Only show the block if the user has view access for the top-level node.
        if ($title) {
          $tree = menu_tree_page_data($node->book['menu_name']);
          // There should only be one element at the top level.
          $data = array_shift($tree);
          if (isset($data['link']['options']) && !is_array($data['link']['options'])) {
            $data['link']['options'] = unserialize($data['link']['options']);
          }
          $block['subject'] = theme('book_title_link', $data['link']);
          $block['content'] = ($data['below']) ? menu_tree_output($data['below']) : '';
        }
      }
      return $block;
    case 'configure':
      $options = array(
        'all pages' => t('Show block on all pages'),
        'book pages' => t('Show block only on book pages'),
      );
      $form['book_block_mode'] = array(
        '#type' => 'radios',
        '#title' => t('Book navigation block display'),
        '#options' => $options,
        '#default_value' => variable_get('book_block_mode', 'all pages'),
        '#description' => t("If <em>Show block on all pages</em> is selected, the block will contain the automatically generated menus for all of the site's books. If <em>Show block only on book pages</em> is selected, the block will contain only the one menu corresponding to the current page's book. In this case, if the current page is not in a book, no block will be displayed. The <em>Page specific visibility settings</em> or other visibility settings can be used in addition to selectively display this block."),
        );
      return $form;
    case 'save':
      variable_set('book_block_mode', $edit['book_block_mode']);
      break;
  }
}

/**
 * Generate the HTML output for a link to a book title when used as a block title.
 *
 * @ingroup themeable
 */
function theme_book_title_link($link) {
  $link['options']['attributes']['class'] = 'book-title';
  return l($link['title'], $link['href'], $link['options']);
}

/**
 * Returns an array of all books.
 *
 * This list may be used for generating a list of all the books, or for building
 * the options for a form select.
 */
function book_get_books() {
  static $all_books;

  if (!isset($all_books)) {
    $all_books = array();
    $result = db_query("SELECT DISTINCT(bid) FROM {book}");
    $nids = array();
    while ($book = db_fetch_array($result)) {
      $nids[] = $book['bid'];
    }
    if ($nids) {
      $result2 = db_query(db_rewrite_sql("SELECT n.type, n.title, b.*, ml.* FROM {book} b INNER JOIN {node} n on b.nid = n.nid INNER JOIN {menu_links} ml ON b.mlid = ml.mlid WHERE n.nid IN (". implode(',', $nids) .") AND n.status = 1 ORDER BY ml.weight, ml.link_title"));
      while ($link = db_fetch_array($result2)) {
        $link['href'] = $link['link_path'];
        $link['options'] = unserialize($link['options']);
        $all_books[$link['bid']] = $link;
      }
    }
  }
  return $all_books;
}

/**
 * Common helper function to handles additions and updates to the book outline.
 *
 * Performs all additions and updates to the book outline through node addition,
 * node editing, node deletion, or the outline tab.
 */
function _book_update_outline(&$node) {
  if ($mlid = db_result(db_query('SELECT mlid FROM {category_menu_map} WHERE nid = %d', $node->nid))) {
    $node->book['mlid'] = $mlid;
    $behavior = variable_get('category_behavior_'. $node->type, 0);
    if (empty($behavior)) {
      $behavior = 'default';
    }

    switch ($behavior) {
      case 'container':
        $node->book['bid'] = $node->nid;
        break;
        
      case 'category':
        $node->book['bid'] = $node->category['cnid'];
        break;
        
      default:
        if (!empty($node->categories) && is_array($node->categories)) {
          $cid = NULL;
          foreach ($node->categories as $key => $cat) {
            if ($key != 'tags') {
              if (is_array($cat)) {
                foreach ($cat as $curr_cid) {
                  if (!empty($curr_cid)) {
                    $cid = $curr_cid;
                    break 1;
                  }
                }
              }
              else if (isset($cat->cid)) {
                $cid = $cat->cid;
              }
              else {
                $cid = $cat;
              }
              
              break 1;
            }
          }
          
          if (!empty($cid)) {
            $node->book['bid'] = $cid;
          }
        }
        break;
    }
    
    $new = db_result(db_query("SELECT bid FROM {book} WHERE nid = %d", $node->nid));

    if (empty($new)) {
      // Insert new.
      db_query("INSERT INTO {book} (nid, mlid, bid) VALUES (%d, %d, %d)", $node->nid, $node->book['mlid'], $node->book['bid']);
    }
    else {
      db_query("UPDATE {book} SET mlid = %d, bid = %d WHERE nid = %d", $node->book['mlid'], $node->book['bid'], $node->nid);
    }
    return TRUE;
  }
  // Failed to save the book
  return FALSE;
}

/**
 * Implementation of hook_nodeapi().
 *
 * Appends book navigation to all nodes in the book, and handles book outline
 * insertions and updates via the node form.
 */
function book_nodeapi(&$node, $op, $teaser, $page) {
  switch ($op) {
    case 'load':
      $behavior = variable_get('category_behavior_'. $node->type, 0);
      if (!empty($behavior) && empty($node->category['is_legacy'])) {
        // Note - we cannot use book_link_load() because it will call node_load()
        $info['book'] = db_fetch_array(db_query('SELECT b.bid, ml.* FROM {book} b INNER JOIN {menu_links} ml ON b.mlid = ml.mlid WHERE b.nid = %d', $node->nid));
        if ($info['book']) {
          $info['book']['href'] = $info['book']['link_path'];
          $info['book']['title'] = $info['book']['link_title'];
          $info['book']['options'] = unserialize($info['book']['options']);
          return $info;
        }
      }
      break;
    case 'presave':
      // Always save a revision for non-administrators.
      if (!empty($node->book['bid']) && !user_access('administer nodes')) {
        $node->revision = 1;
      }
      // Make sure a new node gets a new menu link.
      if (empty($node->nid)) {
        $node->book['mlid'] = NULL;
      }
      break;
    case 'insert':
    case 'update':
      _book_update_outline($node);
      break;
    case 'delete':
      if ($mlid = db_result(db_query('SELECT mlid FROM {book} WHERE nid = %d', $node->nid))) {
        db_query('DELETE FROM {book} WHERE mlid = %d', $mlid);
      }
      break;
  }
}

/**
 * Implementation of hook_help().
 */
function book_help($path, $arg) {
  switch ($path) {
    case 'admin/help#book':
      return t('<p>This is a wrapper module to provide a compatibility layer between the book module and the category module. Modules that depend on the book module should function correctly with this wrapper module enabled, as it routes all book requests to the category API, and converts category data types into book data types. The book module user interface is not available with this wrapper module: you should use the category module user interface instead. For further assistance, see the <a href="!category">category module help page</a>.</p>', array('!category' => url('admin/help/category')));
  }
}

/**
 * Like menu_link_load(), but adds additional data from the {book} table.
 *
 * Do not call when loading a node, since this function may call node_load().
 */
function book_link_load($mlid) {
  if ($item = db_fetch_array(db_query("SELECT * FROM {menu_links} ml INNER JOIN {book} b ON b.mlid = ml.mlid LEFT JOIN {menu_router} m ON m.path = ml.router_path WHERE ml.mlid = %d", $mlid))) {
    _menu_link_translate($item);
    return $item;
  }
  return FALSE;
}

/**
 * Function to check the status of the wrapper.
 */
function book_wrapper_is_enabled() {
  return TRUE;
}
