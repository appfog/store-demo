<?php
// $Id: category.inc,v 1.72 2009/12/02 00:54:34 jaza Exp $

/**
 * @file
 * API functions for the category system as provided by the category module.
 * This file is required in order for the category module to work.
 *
 * Other modules should only call functions in this file when interfacing
 * with the category system - functions in category.module itself should not
 * need to be called directly.
 */

/**
 * Return an array of all container objects.
 *
 * @param $type
 *   If set, return only those containers associated with this node type.
 */
function category_get_containers($type = NULL) {
  $cache_key = 'c_get_conts'. $type;
  $containers = category_cache_op('get', 0, $cache_key);
  if (!isset($containers)) {

    $type_sql = $type ? " WHERE nt.type = '%s'" : "";
    $result = db_query(db_rewrite_sql("SELECT n.nid, n.title, cn.*, c.weight, c.depth, n.status, nt.type FROM {category_cont} cn INNER JOIN {category} c ON cn.cid = c.cid INNER JOIN {node} n ON cn.cid = n.nid LEFT JOIN {category_cont_node_types} nt ON cn.cid = nt.cid". $type_sql ." ORDER BY c.weight, n.title", 'n', 'nid'), $type);

    $containers = array();
    $node_types = array();
    while ($container = db_fetch_object($result)) {
      // If no node types are associated with a container, the LEFT JOIN will
      // return a NULL value for type.
      if (isset($container->type)) {
        $node_types[$container->cid][$container->type] = $container->type;
        unset($container->type);
        $container->nodes = $node_types[$container->cid];
      }
      elseif (!isset($container->nodes)) {
        $container->nodes = array();
      }

      if (empty($container->admin_title)) {
        $container->admin_title = '';
        $container->has_admin_title = FALSE;
      }
      else {
        $container->has_admin_title = TRUE;
      }
      $containers[$container->cid] = $container;
    }

    category_cache_op('set', 0, $cache_key, $containers);
  }

  return $containers;
}

/**
 * Find all categories associated to the given node, within one container.
 */
function category_node_get_categories_by_container($node, $cnid, $key = 'cid') {
  $cache_key = 'c_node_get_cats_by_cont'. $node->vid .':'. $cnid .':'. $key;
  $categories = category_cache_op('get', $node->nid, $cache_key);
  if (!isset($categories)) {

    $result = db_query(db_rewrite_sql('SELECT c.*, n.title FROM {category} c INNER JOIN {category_node} r ON c.cid = r.cid INNER JOIN {node} n ON c.cid = n.nid WHERE c.cnid = %d AND r.vid = %d AND n.status = 1 ORDER BY c.weight'), $cnid, $node->vid);
    $categories = array();
    while ($category = db_fetch_object($result)) {
      $categories[$category->$key] = $category;
    }

    category_cache_op('set', $node->nid, $cache_key, $categories);
  }
  return $categories;
}

/**
 * Find all categories associated to the given node, ordered by container and category weight.
 */
function category_node_get_categories($node, $key = 'cid') {
  $cache_key = 'cat_node_get_cats'. $node->vid .':'. $key;
  $categories = category_cache_op('get', $node->nid, $cache_key);
  if (!isset($categories)) {

    $categories = array();

    $result = db_query(db_rewrite_sql('SELECT n.nid, nr.teaser AS description, r.vid AS node_id, c.*, n.title FROM {category} c INNER JOIN {category_node} r ON c.cid = r.cid INNER JOIN {category} cn ON c.cnid = cn.cid INNER JOIN {node} n ON c.cid = n.nid INNER JOIN {node_revisions} nr ON n.vid = nr.vid INNER JOIN {node} cnn ON cn.cid = cnn.nid WHERE n.status = 1 AND r.vid = %d ORDER BY cn.weight, cnn.title, c.weight, n.title', 'n', 'nid'), $node->vid);

    while ($category = db_fetch_object($result)) {
      $categories[$category->$key] = $category;
    }

    category_cache_op('set', $node->nid, $cache_key, $categories);
  }
  return $categories;
}

/**
 * Find all category objects related to a given category ID.
 */
function category_get_related($cid, $key = 'cid') {
  if ($cid) {
    $cache_key = 'cat_get_related'. $key;
    $related = category_cache_op('get', $cid, $cache_key);
    if (!isset($related)) {

      $result = db_query(db_rewrite_sql('SELECT c.*, n.title, cid1, cid2 FROM {category_relation}, {category} c, {node} n WHERE (c.cid = cid1 OR c.cid = cid2) AND (cid1 = %d OR cid2 = %d) AND c.cid = n.nid AND c.cid != %d AND n.status = 1 ORDER BY c.weight, n.title'), $cid, $cid, $cid);
      $related = array();
      while ($category = db_fetch_object($result)) {
        $related[$category->$key] = $category;
      }

      category_cache_op('set', $cid, $cache_key, $related);
    }
    return $related;
  }
  else {
    return array();
  }
}

/**
 * Find all parents of a given category ID.
 * This function will return the parents in an indexed array sorted by the parents' weight and title.
 */
function category_get_parents($cid, $key = 'cid', $distant = TRUE) {
  if (!$cid) {
    return array();
  }

  $cache_key = 'cat_get_parents'. ($distant ? 'd' : 'n');
  $parents = category_cache_op('get', 0, $cache_key);
  if (!isset($parents)) {

    $distant_sql = $distant ? '' : ' AND (c.cnid = cc.cnid OR c.cnid = 0)';
    $result = db_query(db_rewrite_sql('SELECT c.*, h.cid AS child, n.title, cn.admin_title FROM {category} c INNER JOIN {category_hierarchy} h ON c.cid = h.parent INNER JOIN {node} n ON c.cid = n.nid LEFT JOIN {category_cont} cn ON c.cid = cn.cid INNER JOIN {category} cc ON h.cid = cc.cid WHERE n.status = 1'. $distant_sql .' ORDER BY c.weight, n.title'), $cid);
    $parents = array();
    while ($parent = db_fetch_object($result)) {
      if (empty($parent->admin_title)) {
        $parent->admin_title = $parent->title;
      }
      $parents[$parent->child][$parent->$key] = $parent;
    }

    category_cache_op('set', 0, $cache_key, $parents);
  }

  $no_parent = new StdClass();
  $no_parent->cid = 0;
  $no_parent->cnid = 0;
  $no_parent->weight = 0;
  $no_parent->depth = 0;
  $no_parent->title = '';
  $no_parent->admin_title = '';
  $no_parent->child = $cid;
  return isset($parents[$cid]) ? $parents[$cid] : array(0 => $no_parent);
}

/**
 * Find all ancestors of a given category ID.
 */
function category_get_parents_all($cid, $distant = FALSE) {
  $parents = array();
  if ($cid) {
    $parents[] = category_get_category($cid);
    $n = 0;
    while ($parent = category_get_parents($parents[$n]->cid, 'cid', $distant)) {
      $parents = array_merge($parents, $parent);
      $n++;
    }
  }
  return $parents;
}

/**
 * Find all children of a category ID.
 */
function category_get_children($cid, $cnid = 0, $key = 'cid') {

  if ($cnid) {
    $cache_key = 'cat_get_children_cn'. $cnid .':'. $key;
    $children = category_cache_op('get', $cid, $cache_key);
    if (!isset($children)) {

      $result = db_query(db_rewrite_sql('SELECT c.*, n.title FROM {category} c INNER JOIN {category_hierarchy} h ON c.cid = h.cid INNER JOIN {node} n ON c.cid = n.nid WHERE h.parent = %d AND n.status = 1 AND c.cnid = %d ORDER BY c.weight, n.title'), $cid, $cnid);

      $children = array();
      while ($category = db_fetch_object($result)) {
        $children[$category->$key] = $category;
      }

      category_cache_op('set', $cid, $cache_key, $children);
    }
    return $children;
  }

  $cache_key = 'cat_get_children_c0'. $key;
  $children = category_cache_op('get', 0, $cache_key);
  if (!isset($children)) {

    $result = db_query(db_rewrite_sql('SELECT c.*, h.parent, n.title FROM {category} c INNER JOIN {category_hierarchy} h ON c.cid = h.cid INNER JOIN {node} n ON c.cid = n.nid WHERE n.status = 1 ORDER BY c.weight, n.title'), $cid);

    $children = array();
    while ($category = db_fetch_object($result)) {
      $children[$category->parent][$category->$key] = $category;
    }

    category_cache_op('set', 0, $cache_key, $children);
  }

  return isset($children[$cid]) ? $children[$cid] : array();
}

/**
 * Create a hierarchical representation of a set of categories.
 *
 * @param $cnid
 *   Which container to generate the tree for.
 *
 * @param $parent
 *   The node ID under which to generate the tree. If 0, generate the tree
 *   for everything under the container.
 *
 * @param $depth
 *   Internal use only.
 *
 * @param $max_depth
 *   The number of levels of the tree to return. Leave NULL to return all levels.
 *
 * @param $distant
 *   Whether other containers or categories under them should be returned as
 *   part of the tree. Default is false.
 *
 * @return
 *   An array of all category objects in the tree. Each category object is extended
 *   to have "depth" and "parents" attributes in addition to its normal ones.
 */
function category_get_tree($cnid, $parent = NULL, $depth = -1, $max_depth = NULL, $distant = FALSE) {
  if (empty($cnid) && $depth == -1) {
    $distant = TRUE;
  }
  if (!isset($parent)) {
    $parent = $cnid;
  }

  $depth++;

  // We cache trees, so it's not CPU-intensive to call get_tree() on a category
  // and its children, too.
  $cache_key = 'cat_get_tree'. ($distant ? 'd' : 'n');
  $data = category_cache_op('get', $cnid, $cache_key);
  if (isset($data)) {
    $children = $data['children'];
    $categories = $data['categories'];
    $parents = $data['parents'];
  }
  else {

    $distant_sql = ($distant) ? '' : 'AND c.cnid = %d ';

    $children = array();
    $categories = array();
    $parents = array();
    $result = db_query(db_rewrite_sql('SELECT c.cid, c.cnid, c.weight, c.depth, h.parent, n.title, cn.admin_title FROM {category} c INNER JOIN {category_hierarchy} h ON c.cid = h.cid INNER JOIN {node} n ON c.cid = n.nid LEFT JOIN {category_cont} cn ON c.cid = cn.cid LEFT JOIN {category} c2 ON h.parent = c2.cid WHERE (c2.cid IS NOT NULL OR h.parent = 0) AND n.status = 1 '. $distant_sql .'ORDER BY c.weight, n.title', 'n', 'nid'), $cnid);

    while ($category = db_fetch_object($result)) {
      if (!$distant) {
        $parent_cat = category_get_category($category->parent);
        if ($parent_cat->cid != $cnid && $parent_cat->cnid != $cnid) {
          $category->parent = $cnid;
        }
      }

      $children[$category->parent][] = $category->cid;
      $categories[$category->cid] = $category;
      $parents[$category->cid][] = $category->parent;
    }
    category_cache_op('set', $cnid, $cache_key, array('children' => $children, 'categories' => $categories, 'parents' => $parents));
  }

  $max_depth = (is_null($max_depth) && !empty($children)) ? count($children) : $max_depth;
  $tree = array();
  if (!empty($children) && !empty($children[$parent])) {
    foreach ($children[$parent] as $child) {
      if ($max_depth > $depth) {
        $cat = drupal_clone($categories[$child]);
        $cat->depth = $depth;
        // The "parent" attribute is not useful, as it would show one parent only.
        unset($cat->parent);
        $cat->parents = $parents[$child];

        if (!$cat->cnid && empty($cat->admin_title)) {
          $cat->admin_title = $cat->title;
        }
        $tree[] = $cat;

        if (!empty($children[$child])) {
          $tree = array_merge($tree, category_get_tree($cnid, $child, $depth, $max_depth, $distant));
        }
      }
    }
  }

  return $tree;
}

/**
 * Return an array of synonyms of the given category ID.
 */
function category_get_synonyms($cid) {
  if ($cid) {
    $cache_key = 'cat_get_synonyms';
    $synonyms = category_cache_op('get', $cid, $cache_key);
    if (!isset($synonyms)) {

      $synonyms = array();
      $result = db_query('SELECT name FROM {category_synonym} WHERE cid = %d', $cid);
      while ($synonym = db_fetch_array($result)) {
        $synonyms[] = $synonym['name'];
      }

    category_cache_op('set', $cid, $cache_key, $synonyms);
    }
    return $synonyms;
  }
  else {
    return array();
  }
}

/**
 * Return the category object that has the same given string as a synonym.
 */
function category_get_synonym_root($synonym) {
  return db_fetch_object(db_query("SELECT c.*, n.title FROM {category} c INNER JOIN {category_synonym} s ON c.cid = s.cid AND s.name = '%s' INNER JOIN {node} n ON c.cid = n.nid", $synonym));
}

/**
 * Given a category id, count the number of published nodes in it.
 */
function category_category_count_nodes($cid, $type = 0) {
  $cache_key = 'cat_cat_count_nodes'. $type;
  $count = category_cache_op('get', 0, $cache_key);
  if (!isset($count)) {
    // $type == 0 always evaluates true if $type is a string
    if (is_numeric($type)) {
      $result = db_query(db_rewrite_sql('SELECT c.cid, COUNT(n.nid) AS cnt FROM {category_node} c INNER JOIN {node} n ON c.nid = n.nid WHERE n.status = 1 GROUP BY c.cid'));
    }
    else {
      $result = db_query(db_rewrite_sql("SELECT c.cid, COUNT(n.nid) AS cnt FROM {category_node} c INNER JOIN {node} n ON c.nid = n.nid WHERE n.status = 1 AND n.type = '%s' GROUP BY c.cid"), $type);
    }
    while ($category = db_fetch_object($result)) {
      $count[$category->cid] = $category->cnt;
    }
    category_cache_op('set', 0, $cache_key, $count);
  }
  $children_count = 0;
  foreach (_category_category_children($cid) as $c) {
    $children_count += category_category_count_nodes($c, $type);
  }
  return $children_count + (isset($count[$cid]) ? $count[$cid] : 0);
}

/**
 * Helper for category_category_count_nodes().
 */
function _category_category_children($cid) {
  $cache_key = 'cat_cat_childr';
  $children = category_cache_op('get', 0, $cache_key);
  if (!isset($children)) {
    $result = db_query('SELECT cid, parent FROM {category_hierarchy}');
    while ($category = db_fetch_object($result)) {
      $children[$category->parent][] = $category->cid;
    }
    category_cache_op('set', 0, $cache_key, $children);
  }
  return isset($children[$cid]) ? $children[$cid] : array();
}

/**
 * Try to map a string to an existing category, as for glossary use.
 *
 * Provides a case-insensitive and trimmed mapping, to maximize the
 * likelihood of a successful match.
 *
 * @param title
 *   Title of the category to search for.
 *
 * @return
 *   An array of matching category objects.
 */
function category_get_category_by_name($title) {
  $db_result = db_query(db_rewrite_sql("SELECT n.nid, n.title, c.* FROM {category} c INNER JOIN {node} n ON c.cid = n.nid WHERE LOWER('%s') LIKE LOWER(n.title)", 'n', 'nid'), trim($title));
  $result = array();
  while ($category = db_fetch_object($db_result)) {
    $result[] = $category;
  }

  return $result;
}

/**
 * Return the container object matching a container ID.
 *
 * @param $cnid
 *   The container's ID
 *
 * @return
 *   The container object with all of its metadata, if exists, NULL otherwise.
 *   Results are cached.
 */
function category_get_container($cnid) {
  $cache_key = 'cat_get_cont';
  $container = category_cache_op('get', $cnid, $cache_key);
  if (!isset($container)) {
    // Initialize so if this container does not exist, we have
    // that cached, and we will not try to load this later.
    $container = FALSE;
    // Try to load the data and fill up the object.
    $result = db_query(db_rewrite_sql('SELECT n.nid, cn.*, n.title, c.weight, c.depth, ct.type, c.weight AS cont_weight, c.depth AS cont_depth FROM {category} c INNER JOIN {category_cont} cn ON c.cid = cn.cid INNER JOIN {node} n ON c.cid = n.nid LEFT JOIN {category_cont_node_types} ct ON c.cid = ct.cid WHERE c.cid = %d ORDER BY c.weight, n.title', 'n', 'nid'), $cnid);
    $node_types = array();

    while ($cont = db_fetch_object($result)) {
      // Find all allowed node types for this container.
      if (!empty($cont->type)) {
        $node_types[$cont->type] = $cont->type;
      }
      unset($cont->type);
      $cont->nodes = $node_types;

      // If no admin title is specified, inherit the container title.
      if (empty($cont->admin_title)) {
        $cont->admin_title = '';
        $cont->has_admin_title = FALSE;
      }
      else {
        $cont->has_admin_title = TRUE;
      }

      $container = $cont;
    }
    category_cache_op('set', $cnid, $cache_key, $container);
  }

  // Return NULL if this vocabulary does not exist.
  return (!empty($container) ? $container : NULL);
}

/**
 * Retrieve the category object matching a category ID.
 *
 * @param $cid
 *   The node/category ID of the category to be retrieved.
 *
 * @return
 *   Populated category object if the category is found, NULL otherwise.
 */
function category_get_category($cid) {
  $cache_key = 'cat_get_cat';
  $category = category_cache_op('get', $cid, $cache_key);
  if (!isset($category)) {
    $category = db_fetch_object(db_query('SELECT n.nid, n.title, c.* FROM {category} c INNER JOIN {node} n ON c.cid = n.nid WHERE cid = %d', $cid));
    if (empty($category)) {
      $category = FALSE;
    }
    category_cache_op('set', $cid, $cache_key, $category);
  }

  return (!empty($category) ? $category : NULL);
}

/**
 * Finds all nodes that match selected category conditions.
 *
 * @param $cids
 *   An array of category IDs to match.
 * @param $operator
 *   How to interpret multiple IDs in the array. Can be "or" or "and".
 * @param $depth
 *   How many levels deep to traverse the category tree. Can be a nonnegative
 *   integer or "all".
 * @param $pager
 *   Whether the nodes are to be used with a pager (the case on most Drupal
 *   pages) or not (in an XML feed, for example).
 * @param $distant
 *   Whether other containers or categories under them should be returned as
 *   part of the tree. Default is false.
 * @param $order
 *   The order clause for the query that retrieve the nodes.
 * @return
 *   A resource identifier pointing to the query results.
 */
function category_select_nodes($cids = array(), $operator = 'or', $depth = 0, $pager = TRUE, $distant = FALSE, $order = 'n.sticky DESC, n.created DESC') {
  if (count($cids) > 0) {
    // For each category ID, generate an array of descendant category IDs to the right depth.
    $descendant_cids = array();
    if ($depth === 'all' || $depth < 0) {
      $depth = NULL;
    }
    foreach ($cids as $index => $cid) {
      $category = category_get_category($cid);
      $cnid = $category->cnid ? $category->cnid : $cid;
      $tree = category_get_tree($cnid, $cid, -1, $depth, $distant);
      $descendant_cids[] = array_merge(array($cid), array_map('_category_get_cid_from_category', $tree));
    }

    $args = array();
    if ($operator == 'or') {
      $args = call_user_func_array('array_merge', $descendant_cids);
      $placeholders = db_placeholders($args, 'int');
      $sql = 'SELECT DISTINCT(n.nid), n.sticky, n.title, n.created FROM {node} n INNER JOIN {category_node} cn ON n.nid = cn.nid WHERE cn.cid IN ('. $placeholders .') AND n.status = 1 ORDER BY '. $order;
      $sql_count = 'SELECT COUNT(DISTINCT(n.nid)) FROM {node} n INNER JOIN {category_node} cn ON n.nid = cn.nid WHERE cn.cid IN ('. $placeholders .') AND n.status = 1';
    }
    else {
      $joins = '';
      $wheres = '';
      foreach ($descendant_cids as $index => $cids) {
        $joins .= ' INNER JOIN {category_node} cn'. $index .' ON n.nid = cn'. $index .'.nid';
        $wheres .= ' AND cn'. $index .'.cid IN ('. db_placeholders($cids, 'int') .')';
        $args = array_merge($args, $cids);
      }
      $sql = 'SELECT DISTINCT(n.nid), n.sticky, n.title, n.created FROM {node} n '. $joins .' WHERE n.status = 1 '. $wheres .' ORDER BY '. $order;
      $sql_count = 'SELECT COUNT(n.nid) FROM {node} n '. $joins .' WHERE n.status = 1 ' . $wheres;
    }
    $sql = db_rewrite_sql($sql);
    $sql_count = db_rewrite_sql($sql_count);
    if ($pager) {
      $result = pager_query($sql, variable_get('default_nodes_main', 10), 0, $sql_count, $args);
    }
    else {
      $result = db_query_range($sql, $args, 0, variable_get('feed_default_items', 10));
    }
  }

  return $result;
}

/**
 * Accepts the result of a pager_query() call, such as that performed by
 * category_select_nodes(), and formats each node along with a pager.
 */
function category_render_nodes($result, $category_display = NULL) {
  $output = '';
  $has_rows = FALSE;
  while ($node = db_fetch_object($result)) {
    $output .= node_view(node_load($node->nid), 1);
    $has_rows = TRUE;
  }
  if ($has_rows) {
    $output .= theme('pager', NULL, variable_get('default_nodes_main', 10), 0);
  }
  else {
    if (!module_exists('category_display') || !empty($category_display->emptymsg)) {
      $output .= '<p>'. t('There are currently no posts in this category.') .'</p>';
    }
  }

  return $output;
}

/**
 * Parses a comma or plus separated string of category IDs.
 *
 * @param $str_cids
 *   A string of category IDs, separated by plus or comma.
 *   comma (,) means AND
 *   plus (+) means OR
 *
 * @return an associative array with an operator key (either 'and'
 *   or 'or') and a cid key containing an array of the category IDs.
 */
function category_categories_parse_string($str_cids) {
  $categories = array('operator' => '', 'cids' => array());
  if (preg_match('/^([0-9]+[+ ])+[0-9]+$/', $str_cids)) {
    $categories['operator'] = 'or';
    // The '+' character in a query string may be parsed as ' '.
    $categories['cids'] = preg_split('/[+ ]/', $str_cids);
  }
  else if (preg_match('/^([0-9]+,)*[0-9]+$/', $str_cids)) {
    $categories['operator'] = 'and';
    $categories['cids'] = explode(',', $str_cids);
  }
  return $categories;
}

/**
 * Central caching function for all kinds of node-related category data,
 * so that all data for a given node may be loaded by single query from
 * cache table into static variable, and then returned by various API
 * functions without any queries needed. The cache is stored per-node,
 * and flushed only when given node is saved or deleted. The $nid of '0'
 * is used for global summary-data, and flushed on every node save.
 * While saving a category or container, we flush the whole cache, as
 * this affects the whole hiearchy, and happens rarely. This should not
 * be used for data affected by other factors (such as current user's
 * permissions), unless the condition is incorporated into the cache key.
 *
 * @param op
 *   Operation to be performed - either 'get', 'set', or 'flush'.
 *
 * @param nid
 *   Node id to handle data for. 0 is general data, 'all' flushes all.
 *
 * @param key
 *   Name of node-data subset to work with. (Usually an API function identificator.)
 *
 * @param data
 *   Data for the 'set' operation (will get added to the existing per-node record).
 *
 * @return
 *   Cached data, or NULL if none.
 */
function category_cache_op($op, $nid, $key = '', $data = NULL) {
  static $cache;
  if (!isset($cache)) {
    $cache = array('n' => array(), 'g' => array());
  }

  // Flush cache if requested
  if ($op == 'flush') {
    if ($nid === 'all') {
      // Total flush: Empty {cache_category} and static cache entirely
      cache_clear_all('*', 'cache_category', TRUE);
      $cache = array('n' => array(), 'g' => array());
    }
    else {
      // Node flush: Delete entries for given node
      cache_clear_all('n'. $nid, 'cache_category');
      unset($cache['n'][$nid]);
      // Delete also all entries for global data, because we don't really
      // know how these might be affected.
      cache_clear_all('g', 'cache_category', TRUE);
      $cache['g'] = array();
    }
    return;
  }

  // For global data, we use the $key instead of $nid as identificator, for
  // better granularity of data loading (as these are larger pieces). Both
  // node/global parts of the cache are separated, to allow for all-global
  // flush.
  $part = ($nid == 0) ? 'g' : 'n';
  $id = ($nid == 0) ? $key : $nid;

  // Load existing record, if not available in static variable already.
  if (!isset($cache[$part][$id])) {
    $entry = cache_get($part . $id, 'cache_category');
    if ($entry) {
      $cache[$part][$id] = $entry->data;
    }
    else {
      $cache[$part][$id] = array();
    }
  }

  // Save new data if required.
  if ($op == 'set') {
    // Store serialized data, to ensure a fresh copy being always returned from
    // static cache, without any references to the original (which might be
    // further changed, after it got set to the cache). This is to avoid issues
    // with PHP5 always passing and assigning objects as references, and/or with
    // any nested by-reference elements.
    $cache[$part][$id][$key] = serialize($data);
    cache_set($part . $id, $cache[$part][$id], 'cache_category');
  }

  // Return data for given node/key combination.
  return isset($cache[$part][$id][$key]) ? unserialize($cache[$part][$id][$key]) : NULL;
}
