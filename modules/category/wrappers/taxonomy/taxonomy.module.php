<?php
// $Id: taxonomy.module.php,v 1.7 2009/12/02 00:54:34 jaza Exp $

/**
 * @file
 * Wrapper module to provide a compatibility layer between the taxonomy
 * module and the category module.
 */

/**
 * Implementation of hook_theme()
 */
function taxonomy_theme() {
  return array(
    'taxonomy_term_page' => array(
      'arguments' => array('tids' => array(), 'result' => NULL),
    ),
    'taxonomy_overview_vocabularies' => array(
      'arguments' => array('form' => array()),
    ),
    'taxonomy_overview_terms' => array(
      'arguments' => array('form' => array()),
    ),
  );
}

/**
 * Implementation of hook_link().
 *
 * This hook is extended with $type = 'taxonomy terms' to allow themes to
 * print lists of terms associated with a node. Themes can print taxonomy
 * links with:
 *
 * if (module_exists('taxonomy')) {
 *   $terms = taxonomy_link('taxonomy terms', $node);
 *   print theme('links', $terms);
 * }
 */
function taxonomy_link($type, $node = NULL) {
  if ($type == 'taxonomy terms' && $node != NULL) {
    // Change 'type' to what the category module recognises.
    $type = 'categories';

    $links = category_link($type, $node);

    // No point invoking link_alter here, because category_link() has already
    // done it, and because modules and themes looking for $links[taxonomy_*]
    // won't find it anyway.

    return $links;
  }
}

/**
 * For vocabularies not maintained by taxonomy.module, give the maintaining
 * module a chance to provide a path for terms in that vocabulary.
 *
 * @param $term
 *   A term object.
 * @return
 *   An internal Drupal path.
 */

function taxonomy_term_path($term) {
  $vocabulary = taxonomy_vocabulary_load($term->vid);
  if ($vocabulary->module != 'category' && $path = module_invoke($vocabulary->module, 'term_path', $term)) {
    return $path;
  }
  return 'node/'. $term->tid;
}

/**
 * Implementation of hook_menu().
 */
function taxonomy_menu() {
  $items = array();

  $items['taxonomy/term/%'] = array(
    'title' => 'Taxonomy term',
    'page callback' => 'taxonomy_term_page',
    'page arguments' => array(2),
    'access arguments' => array('access content'),
    'type' => MENU_CALLBACK,
  );

  return $items;
}

/**
 * Menu callback; shows a taxonomy term's new category page.
 */
function taxonomy_term_page($str_tids = '', $depth = 0, $op = 'page') {
  module_load_include('inc', 'category', 'category.pages');
  return category_page($str_tids, $depth, $op);
}

/**
 * Save a vocabulary.
 */
function taxonomy_save_vocabulary(&$edit, $nodeapi_call = FALSE) {
  if (!$nodeapi_call) {
    $node = _taxonomy_vocabulary_into_container($edit);
    node_save($node);
    $edit['vid'] = $node->nid;
  }

  $edit['nodes'] = empty($edit['nodes']) ? array() : $edit['nodes'];

  if (!isset($edit['module'])) {
    $edit['module'] = 'taxonomy';
  }

  if (!empty($edit['vid']) && !empty($edit['name'])) {
    db_query("DELETE FROM {vocabulary} WHERE vid = %d", $edit['vid']);
    drupal_write_record('vocabulary', $edit);
    db_query("DELETE FROM {vocabulary_node_types} WHERE vid = %d", $edit['vid']);
    foreach ($edit['nodes'] as $type => $selected) {
      if (!empty($selected)) {
        db_query("INSERT INTO {vocabulary_node_types} (vid, type) VALUES (%d, '%s')", $edit['vid'], $type);
      }
    }
    module_invoke_all('taxonomy', 'update', 'vocabulary', $edit);
    $status = SAVED_UPDATED;
  }
  else if (!empty($edit['vid'])) {
    $status = taxonomy_del_vocabulary($edit['vid']);
  }
  else {
    drupal_write_record('vocabulary', $edit);
    foreach ($edit['nodes'] as $type => $selected) {
      if (!empty($selected)) {
        db_query("INSERT INTO {vocabulary_node_types} (vid, type) VALUES (%d, '%s')", $edit['vid'], $type);
      }
    }
    module_invoke_all('taxonomy', 'insert', 'vocabulary', $edit);
    $status = SAVED_NEW;
  }

  cache_clear_all();

  return $status;
}

/**
 * Delete a vocabulary.
 *
 * @param $vid
 *   A vocabulary ID.
 * @return
 *   Constant indicating items were deleted.
 */
function taxonomy_del_vocabulary($vid, $nodeapi_call = FALSE) {
  if (!$nodeapi_call) {
    node_delete($vid);
  }

  $vocabulary = (array) taxonomy_vocabulary_load($vid);

  db_query('DELETE FROM {vocabulary} WHERE vid = %d', $vid);
  db_query('DELETE FROM {vocabulary_node_types} WHERE vid = %d', $vid);
  $result = db_query('SELECT tid FROM {term_data} WHERE vid = %d', $vid);
  while ($term = db_fetch_object($result)) {
    taxonomy_del_term($term->tid);
  }

  module_invoke_all('taxonomy', 'delete', 'vocabulary', $vocabulary);

  cache_clear_all();

  return SAVED_DELETED;
}

/**
 * Helper function for taxonomy_form_term_submit().
 *
 * @param $form_state['values']
 * @return
 *   Status constant indicating if term was inserted or updated.
 */
function taxonomy_save_term(&$form_values, $nodeapi_call = FALSE) {
  if (!$nodeapi_call) {
    $node = _taxonomy_term_into_category($form_values);
    node_save($node);
    $form_values['tid'] = $node->nid;
  }

  $form_values += array(
    'description' => '',
    'weight' => 0
  );

  if (!empty($form_values['tid']) && $form_values['name']) {
    db_query("DELETE FROM {term_data} WHERE tid = %d", $form_values['tid']);
    drupal_write_record('term_data', $form_values);
    $hook = 'update';
    $status = SAVED_UPDATED;
  }
  else if (!empty($form_values['tid'])) {
    return taxonomy_del_term($form_values['tid']);
  }
  else {
    drupal_write_record('term_data', $form_values);
    $hook = 'insert';
    $status = SAVED_NEW;
  }

  db_query('DELETE FROM {term_relation} WHERE tid1 = %d OR tid2 = %d', $form_values['tid'], $form_values['tid']);
  if (!empty($form_values['relations'])) {
    foreach ($form_values['relations'] as $related_id) {
      if ($related_id != 0) {
        db_query('INSERT INTO {term_relation} (tid1, tid2) VALUES (%d, %d)', $form_values['tid'], $related_id);
      }
    }
  }

  db_query('DELETE FROM {term_hierarchy} WHERE tid = %d', $form_values['tid']);
  if (!isset($form_values['parent']) || empty($form_values['parent'])) {
    $form_values['parent'] = array(0);
  }
  if (is_array($form_values['parent'])) {
    foreach ($form_values['parent'] as $parent) {
      if (is_array($parent)) {
        foreach ($parent as $tid) {
          db_query('INSERT INTO {term_hierarchy} (tid, parent) VALUES (%d, %d)', $form_values['tid'], $tid);
        }
      }
      else {
        db_query('INSERT INTO {term_hierarchy} (tid, parent) VALUES (%d, %d)', $form_values['tid'], $parent);
      }
    }
  }
  else {
    db_query('INSERT INTO {term_hierarchy} (tid, parent) VALUES (%d, %d)', $form_values['tid'], $form_values['parent']);
  }

  db_query('DELETE FROM {term_synonym} WHERE tid = %d', $form_values['tid']);
  if (!empty($form_values['synonyms'])) {
    foreach (explode ("\n", str_replace("\r", '', $form_values['synonyms'])) as $synonym) {
      if ($synonym) {
        db_query("INSERT INTO {term_synonym} (tid, name) VALUES (%d, '%s')", $form_values['tid'], chop($synonym));
      }
    }
  }

  if (isset($hook)) {
    module_invoke_all('taxonomy', $hook, 'term', $form_values);
  }

  return $status;
}

/**
 * Delete a term.
 *
 * @param $tid
 *   The term ID.
 * @return
 *   Status constant indicating deletion.
 */
function taxonomy_del_term($tid, $nodeapi_call = FALSE) {
  if (!$nodeapi_call) {
    node_delete($tid);
  }

  $tids = array($tid);
  while ($tids) {
    $children_tids = $orphans = array();
    foreach ($tids as $tid) {
      // See if any of the term's children are about to be become orphans:
      if ($children = taxonomy_get_children($tid)) {
        foreach ($children as $child) {
          // If the term has multiple parents, we don't delete it.
          $parents = taxonomy_get_parents($child->tid);
          if (count($parents) == 1) {
            $orphans[] = $child->tid;
          }
        }
      }

      $term = (array) taxonomy_get_term($tid);

      db_query('DELETE FROM {term_data} WHERE tid = %d', $tid);
      db_query('DELETE FROM {term_hierarchy} WHERE tid = %d', $tid);
      db_query('DELETE FROM {term_relation} WHERE tid1 = %d OR tid2 = %d', $tid, $tid);
      db_query('DELETE FROM {term_synonym} WHERE tid = %d', $tid);
      db_query('DELETE FROM {term_node} WHERE tid = %d', $tid);

      module_invoke_all('taxonomy', 'delete', 'term', $term);
    }

    $tids = $orphans;
  }

  cache_clear_all();

  return SAVED_DELETED;
}

/**
 * Return an array of all vocabulary objects.
 *
 * @param $type
 *   If set, return only those vocabularies associated with this node type.
 */
function taxonomy_get_vocabularies($type = NULL) {
  $cache_key = 'tax_get_vocabs'. $type;
  $vocabularies = category_cache_op('get', 0, $cache_key);
  if (!isset($vocabularies)) {

    if ($type) {
      $result = db_query(db_rewrite_sql("SELECT v.vid, v.*, n.type FROM {vocabulary} v LEFT JOIN {vocabulary_node_types} n ON v.vid = n.vid WHERE n.type = '%s' ORDER BY v.weight, v.name", 'v', 'vid'), $type);
    }
    else {
      $result = db_query(db_rewrite_sql('SELECT v.*, n.type FROM {vocabulary} v LEFT JOIN {vocabulary_node_types} n ON v.vid = n.vid ORDER BY v.weight, v.name', 'v', 'vid'));
    }

    $vocabularies = array();
    $node_types = array();
    while ($voc = db_fetch_object($result)) {
      // If no node types are associated with a vocabulary, the LEFT JOIN will
      // return a NULL value for type.
      if (isset($voc->type)) {
        $node_types[$voc->vid][$voc->type] = $voc->type;
        unset($voc->type);
        $voc->nodes = $node_types[$voc->vid];
      }
      elseif (!isset($voc->nodes)) {
        $voc->nodes = array();
      }
      $vocabularies[$voc->vid] = $voc;
    }
  category_cache_op('set', 0, $cache_key, $vocabularies);
  }


  return $vocabularies;
}

/**
 * Find all terms associated with the given node, within one vocabulary.
 */
function taxonomy_node_get_terms_by_vocabulary($node, $vid, $key = 'tid') {
  $cache_key = 'tax_node_get_terms_by_voc'. $node->vid .':'. $vid .':'. $key;
  $terms = category_cache_op('get', $node->nid, $cache_key);
  if (!isset($terms)) {

    $result = db_query(db_rewrite_sql('SELECT t.tid, t.* FROM {term_data} t INNER JOIN {term_node} r ON r.tid = t.tid WHERE t.vid = %d AND r.vid = %d ORDER BY weight', 't', 'tid'), $vid, $node->vid);
    $terms = array();
    while ($term = db_fetch_object($result)) {
      $terms[$term->$key] = $term;
    }

    category_cache_op('set', $node->nid, $cache_key, $terms);
  }
  return $terms;
}

/**
 * Find all terms associated with the given node, ordered by vocabulary and term weight.
 */
function taxonomy_node_get_terms($node, $key = 'tid') {
  $cache_key = 'tax_node_get_terms'. $node->vid .':'. $key;
  $terms = category_cache_op('get', $node->nid, $cache_key);
  if (!isset($terms)) {

    $terms = array();

    $result = db_query(db_rewrite_sql('SELECT t.* FROM {term_node} r INNER JOIN {term_data} t ON r.tid = t.tid INNER JOIN {vocabulary} v ON t.vid = v.vid WHERE r.vid = %d ORDER BY v.weight, t.weight, t.name', 't', 'tid'), $node->vid);
    while ($term = db_fetch_object($result)) {
      $terms[$term->$key] = $term;
    }

    category_cache_op('set', $node->nid, $cache_key, $terms);
  }
  return $terms;
}

/**
 * Save term associations for a given node.
 */
function taxonomy_node_save($node, $terms) {

  taxonomy_node_delete_revision($node);

  // Free tagging vocabularies do not send their tids in the form,
  // so we'll detect them here and process them independently.
  if (isset($terms['tags'])) {
    $typed_input = $terms['tags'];
    unset($terms['tags']);

    foreach ($typed_input as $vid => $vid_value) {
      $typed_terms = drupal_explode_tags($vid_value);

      $inserted = array();
      foreach ($typed_terms as $typed_term) {
        // See if the term exists in the chosen vocabulary
        // and return the tid; otherwise, add a new record.
        $possibilities = taxonomy_get_term_by_name($typed_term);
        $typed_term_tid = NULL; // tid match, if any.
        foreach ($possibilities as $possibility) {
          if ($possibility->vid == $vid) {
            $typed_term_tid = $possibility->tid;
          }
        }

        if (!$typed_term_tid) {
          $edit = array('vid' => $vid, 'name' => $typed_term);
          $status = taxonomy_save_term($edit);
          $typed_term_tid = $edit['tid'];
        }

        // Defend against duplicate, differently cased tags
        if (!isset($inserted[$typed_term_tid])) {
          db_query('INSERT INTO {term_node} (nid, vid, tid) VALUES (%d, %d, %d)', $node->nid, $node->vid, $typed_term_tid);
          $inserted[$typed_term_tid] = TRUE;
        }
      }
    }
  }

  if (is_array($terms)) {
    foreach ($terms as $term) {
      if (is_array($term)) {
        foreach ($term as $tid) {
          if ($tid) {
            db_query('INSERT INTO {term_node} (nid, vid, tid) VALUES (%d, %d, %d)', $node->nid, $node->vid, $tid);
          }
        }
      }
      else if (is_object($term)) {
        if (!empty($term->cid)) {
          $term->tid = $term->cid;
        }
        db_query('INSERT INTO {term_node} (nid, vid, tid) VALUES (%d, %d, %d)', $node->nid, $node->vid, $term->tid);
      }
      else if ($term) {
        db_query('INSERT INTO {term_node} (nid, vid, tid) VALUES (%d, %d, %d)', $node->nid, $node->vid, $term);
      }
    }
  }
}

/**
 * Remove associations of a node to its terms.
 */
function taxonomy_node_delete($node) {
  db_query('DELETE FROM {term_node} WHERE nid = %d', $node->nid);
}

/**
 * Remove associations of a node to its terms.
 */
function taxonomy_node_delete_revision($node) {
  db_query('DELETE FROM {term_node} WHERE vid = %d', $node->vid);
}

/**
 * Implementation of hook_node_type().
 */
function taxonomy_node_type($op, $info) {
  if ($op == 'update' && !empty($info->old_type) && $info->type != $info->old_type) {
    db_query("UPDATE {vocabulary_node_types} SET type = '%s' WHERE type = '%s'", $info->type, $info->old_type);
  }
  elseif ($op == 'delete') {
    db_query("DELETE FROM {vocabulary_node_types} WHERE type = '%s'", $info->type);
  }
}

/**
 * Find all term objects related to a given term ID.
 */
function taxonomy_get_related($tid, $key = 'tid') {
  if ($tid) {
    $cache_key = 'tax_get_related'. $key;
    $related = category_cache_op('get', $tid, $cache_key);
    if (!isset($related)) {

      $result = db_query('SELECT t.*, tid1, tid2 FROM {term_relation}, {term_data} t WHERE (t.tid = tid1 OR t.tid = tid2) AND (tid1 = %d OR tid2 = %d) AND t.tid != %d ORDER BY weight, name', $tid, $tid, $tid);
      $related = array();
      while ($term = db_fetch_object($result)) {
        $related[$term->$key] = $term;
      }

      category_cache_op('set', $tid, $cache_key, $related);
    }
    return $related;
  }
  else {
    return array();
  }
}

/**
 * Find all parents of a given term ID.
 */
function taxonomy_get_parents($tid, $key = 'tid') {
  if ($tid) {
    $cache_key = 'tax_get_parents'. $key;
    $parents = category_cache_op('get', $tid, $cache_key);
    if (!isset($parents)) {

      $result = db_query(db_rewrite_sql('SELECT t.tid, t.* FROM {term_data} t INNER JOIN {term_hierarchy} h ON h.parent = t.tid WHERE h.tid = %d ORDER BY weight, name', 't', 'tid'), $tid);
      $parents = array();
      while ($parent = db_fetch_object($result)) {
        $parents[$parent->$key] = $parent;
      }

      category_cache_op('set', $tid, $cache_key, $parents);
    }
    return $parents;
  }
  else {
    return array();
  }
}

/**
 * Find all ancestors of a given term ID.
 */
function taxonomy_get_parents_all($tid) {
  $parents = array();
  if ($tid) {
    $parents[] = taxonomy_get_term($tid);
    $n = 0;
    while ($parent = taxonomy_get_parents($parents[$n]->tid)) {
      $parents = array_merge($parents, $parent);
      $n++;
    }
  }
  return $parents;
}

/**
 * Find all children of a term ID.
 */
function taxonomy_get_children($tid, $vid = 0, $key = 'tid') {
  $cache_key = 'tax_get_children'. $vid .':'. $key;
  $children = category_cache_op('get', $tid, $cache_key);
  if (!isset($children)) {

    if ($vid) {
      $result = db_query(db_rewrite_sql('SELECT t.* FROM {term_data} t INNER JOIN {term_hierarchy} h ON h.tid = t.tid WHERE t.vid = %d AND h.parent = %d ORDER BY weight, name', 't', 'tid'), $vid, $tid);
    }
    else {
      $result = db_query(db_rewrite_sql('SELECT t.* FROM {term_data} t INNER JOIN {term_hierarchy} h ON h.tid = t.tid WHERE parent = %d ORDER BY weight, name', 't', 'tid'), $tid);
    }
    $children = array();
    while ($term = db_fetch_object($result)) {
      $children[$term->$key] = $term;
    }

    category_cache_op('set', $tid, $cache_key, $children);
  }
  return $children;
}

/**
 * Create a hierarchical representation of a vocabulary.
 *
 * @param $vid
 *   Which vocabulary to generate the tree for.
 *
 * @param $parent
 *   The term ID under which to generate the tree. If 0, generate the tree
 *   for the entire vocabulary.
 *
 * @param $depth
 *   Internal use only.
 *
 * @param $max_depth
 *   The number of levels of the tree to return. Leave NULL to return all levels.
 *
 * @return
 *   An array of all term objects in the tree. Each term object is extended
 *   to have "depth" and "parents" attributes in addition to its normal ones.
 *   Results are statically cached.
 */
function taxonomy_get_tree($vid, $parent = 0, $depth = -1, $max_depth = NULL) {

  $depth++;

  // We cache trees, so it's not CPU-intensive to call get_tree() on a term
  // and its children, too.
  $cache_key = 'tax_get_tree';
  $data = category_cache_op('get', $vid, $cache_key);
  if (isset($data)) {
    $children = $data['children'];
    $terms = $data['terms'];
    $parents = $data['parents'];
  }
  else {
    $children = array();
    $terms = array();
    $parents = array();

    $result = db_query(db_rewrite_sql('SELECT t.tid, t.*, parent FROM {term_data} t INNER JOIN {term_hierarchy} h ON t.tid = h.tid WHERE t.vid = %d ORDER BY weight, name', 't', 'tid'), $vid);
    while ($term = db_fetch_object($result)) {
      $children[$term->parent][] = $term->tid;
      $parents[$term->tid][] = $term->parent;
      $terms[$term->tid] = $term;
    }
    category_cache_op('set', $vid, $cache_key, array('children' => $children, 'terms' => $terms, 'parents' => $parents));
  }

  $max_depth = (is_null($max_depth)) ? count($children) : $max_depth;
  $tree = array();
  if (!empty($children[$parent])) {
    foreach ($children[$parent] as $child) {
      if ($max_depth > $depth) {
        $term = drupal_clone($terms[$child]);
        $term->depth = $depth;
        // The "parent" attribute is not useful, as it would show one parent only.
        unset($term->parent);
        $term->parents = $parents[$child];
        $tree[] = $term;

        if (!empty($children[$child])) {
          $tree = array_merge($tree, taxonomy_get_tree($vid, $child, $depth, $max_depth));
        }
      }
    }
  }

  return $tree;
}

/**
 * Return an array of synonyms of the given term ID.
 */
function taxonomy_get_synonyms($tid) {
  if ($tid) {
    $cache_key = 'tax_get_synonyms';
    $synonyms = category_cache_op('get', $tid, $cache_key);
    if (!isset($synonyms)) {

      $synonyms = array();
      $result = db_query('SELECT name FROM {term_synonym} WHERE tid = %d', $tid);
      while ($synonym = db_fetch_array($result)) {
        $synonyms[] = $synonym['name'];
      }

    category_cache_op('set', $tid, $cache_key, $synonyms);
    }
    return $synonyms;
  }
  else {
    return array();
  }
}

/**
 * Return the term object that has the given string as a synonym.
 */
function taxonomy_get_synonym_root($synonym) {
  return db_fetch_object(db_query("SELECT * FROM {term_synonym} s, {term_data} t WHERE t.tid = s.tid AND s.name = '%s'", $synonym));
}

/**
 * Count the number of published nodes classified by a term.
 *
 * @param $tid
 *   The term's ID
 *
 * @param $type
 *   The $node->type. If given, taxonomy_term_count_nodes only counts
 *   nodes of $type that are classified with the term $tid.
 *
 * @return int
 *   An integer representing a number of nodes.
 *   Results are statically cached.
 */
function taxonomy_term_count_nodes($tid, $type = 0) {
  $cache_key = 'tax_term_count_nodes'. $type;
  $count = category_cache_op('get', 0, $cache_key);
  if (!isset($count)) {

    // $type == 0 always evaluates TRUE if $type is a string
    if (is_numeric($type)) {
      $result = db_query(db_rewrite_sql('SELECT t.tid, COUNT(n.nid) AS c FROM {term_node} t INNER JOIN {node} n ON t.vid = n.vid WHERE n.status = 1 GROUP BY t.tid'));
    }
    else {
      $result = db_query(db_rewrite_sql("SELECT t.tid, COUNT(n.nid) AS c FROM {term_node} t INNER JOIN {node} n ON t.vid = n.vid WHERE n.status = 1 AND n.type = '%s' GROUP BY t.tid"), $type);
    }
    while ($term = db_fetch_object($result)) {
      $count[$term->tid] = $term->c;
    }

    category_cache_op('set', 0, $cache_key, $count);
  }

  $children_count = 0;
  foreach (_taxonomy_term_children($tid) as $c) {
    $children_count += taxonomy_term_count_nodes($c, $type);
  }
  return $children_count + (isset($count[$tid]) ? $count[$tid] : 0);
}

/**
 * Helper for taxonomy_term_count_nodes(). Used to find out
 * which terms are children of a parent term.
 *
 * @param $tid
 *   The parent term's ID
 *
 * @return array
 *   An array of term IDs representing the children of $tid.
 *   Results are statically cached.
 *
 */
function _taxonomy_term_children($tid) {
  $cache_key = 'tax_term_childr';
  $children = category_cache_op('get', 0, $cache_key);
  if (!isset($children)) {
    $result = db_query('SELECT tid, parent FROM {term_hierarchy}');
    while ($term = db_fetch_object($result)) {
      $children[$term->parent][] = $term->tid;
    }
    category_cache_op('set', 0, $cache_key, $children);
  }
  return isset($children[$tid]) ? $children[$tid] : array();
}

/**
 * Try to map a string to an existing term, as for glossary use.
 *
 * Provides a case-insensitive and trimmed mapping, to maximize the
 * likelihood of a successful match.
 *
 * @param name
 *   Name of the term to search for.
 *
 * @return
 *   An array of matching term objects.
 */
function taxonomy_get_term_by_name($name) {
  $db_result = db_query(db_rewrite_sql("SELECT t.tid, t.* FROM {term_data} t WHERE LOWER(t.name) LIKE LOWER('%s')", 't', 'tid'), trim($name));
  $result = array();
  while ($term = db_fetch_object($db_result)) {
    $result[] = $term;
  }

  return $result;
}

/**
 * Return the vocabulary object matching a vocabulary ID.
 *
 * @param $vid
 *   The vocabulary's ID
 *
 * @return
 *   The vocabulary object with all of its metadata, if exists, NULL otherwise.
 *   Results are statically cached.
 */
function taxonomy_vocabulary_load($vid) {
  $cache_key = 'tax_vocab_load';
  $vocab = category_cache_op('get', $vid, $cache_key);
  if (!isset($vocab)) {

    // Initialize so if this vocabulary does not exist, we have
    // that cached, and we will not try to load this later.
    $vocab = FALSE;
    // Try to load the data and fill up the object.
    $result = db_query('SELECT v.*, n.type FROM {vocabulary} v LEFT JOIN {vocabulary_node_types} n ON v.vid = n.vid WHERE v.vid = %d', $vid);
    $node_types = array();
    while ($voc = db_fetch_object($result)) {
      if (!empty($voc->type)) {
        $node_types[$voc->type] = $voc->type;
      }
      unset($voc->type);
      $voc->nodes = $node_types;
      $vocab = $voc;
    }
    category_cache_op('set', $vid, $cache_key, $vocab);
  }

  // Return NULL if this vocabulary does not exist.
  return !empty($vocab) ? $vocab : NULL;
}

/**
 * Return the term object matching a term ID.
 *
 * @param $tid
 *   A term's ID
 *
 * @return Object
 *   A term object. Results are statically cached.
 */
function taxonomy_get_term($tid) {
  $cache_key = 'tax_get_term';
  $term = category_cache_op('get', $tid, $cache_key);
  if (!isset($term)) {
    $term = db_fetch_object(db_query('SELECT * FROM {term_data} WHERE tid = %d', $tid));
    category_cache_op('set', $tid, $cache_key, $term);
  }

  return $term;
}

/**
 * Finds all nodes that match selected taxonomy conditions.
 *
 * @param $tids
 *   An array of term IDs to match.
 * @param $operator
 *   How to interpret multiple IDs in the array. Can be "or" or "and".
 * @param $depth
 *   How many levels deep to traverse the taxonomy tree. Can be a nonnegative
 *   integer or "all".
 * @param $pager
 *   Whether the nodes are to be used with a pager (the case on most Drupal
 *   pages) or not (in an XML feed, for example).
 * @param $order
 *   The order clause for the query that retrieve the nodes.
 * @return
 *   A resource identifier pointing to the query results.
 */
function taxonomy_select_nodes($tids = array(), $operator = 'or', $depth = 0, $pager = TRUE, $order = 'n.sticky DESC, n.created DESC') {
  if (count($tids) > 0) {
    // For each term ID, generate an array of descendant term IDs to the right depth.
    $descendant_tids = array();
    if ($depth === 'all') {
      $depth = NULL;
    }
    foreach ($tids as $index => $tid) {
      $term = taxonomy_get_term($tid);
      $tree = taxonomy_get_tree($term->vid, $tid, -1, $depth);
      $descendant_tids[] = array_merge(array($tid), array_map('_taxonomy_get_tid_from_term', $tree));
    }

    if ($operator == 'or') {
      $args = call_user_func_array('array_merge', $descendant_tids);
      $placeholders = db_placeholders($args, 'int');
      $sql = 'SELECT DISTINCT(n.nid), n.sticky, n.title, n.created FROM {node} n INNER JOIN {term_node} tn ON n.vid = tn.vid WHERE tn.tid IN ('. $placeholders .') AND n.status = 1 ORDER BY '. $order;
      $sql_count = 'SELECT COUNT(DISTINCT(n.nid)) FROM {node} n INNER JOIN {term_node} tn ON n.vid = tn.vid WHERE tn.tid IN ('. $placeholders .') AND n.status = 1';
    }
    else {
      $joins = '';
      $wheres = '';
      $args = array();
      foreach ($descendant_tids as $index => $tids) {
        $joins .= ' INNER JOIN {term_node} tn'. $index .' ON n.vid = tn'. $index .'.vid';
        $wheres .= ' AND tn'. $index .'.tid IN ('. db_placeholders($tids, 'int') .')';
        $args = array_merge($args, $tids);
      }
      $sql = 'SELECT DISTINCT(n.nid), n.sticky, n.title, n.created FROM {node} n '. $joins .' WHERE n.status = 1 '. $wheres .' ORDER BY '. $order;
      $sql_count = 'SELECT COUNT(DISTINCT(n.nid)) FROM {node} n '. $joins .' WHERE n.status = 1 '. $wheres;
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
 * taxonomy_select_nodes(), and formats each node along with a pager.
 */
function taxonomy_render_nodes($result) {
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
    $output .= '<p>'. t('There are currently no posts in this category.') .'</p>';
  }
  return $output;
}

/**
 * Implementation of hook_nodeapi().
 */
function taxonomy_nodeapi($node, $op, $arg = 0) {
  switch ($op) {
    case 'load':
      $output['taxonomy'] = taxonomy_node_get_terms($node);
      return $output;

    case 'insert':
    case 'update':
      $behavior = variable_get('category_behavior_'. $node->type, 0);
      if (!empty($behavior) && empty($node->category['is_legacy'])) {
        if ($behavior == 'category') {
          taxonomy_save_term(_taxonomy_category_into_term($node), TRUE);
        }
        else {
          taxonomy_save_vocabulary(_taxonomy_container_into_vocabulary($node), TRUE);
        }
      }
      if (!empty($node->categories)) {
        taxonomy_node_save($node, _category_filter_pick_elements($node->categories));
      }
      break;

    case 'delete':
      $behavior = variable_get('category_behavior_'. $node->type, 0);
      if (!empty($behavior) && empty($node->category['is_legacy'])) {
        if ($behavior == 'category') {
          taxonomy_del_term($node->nid, TRUE);
        }
        else {
          taxonomy_del_vocabulary($node->nid, TRUE);
        }
      }
      taxonomy_node_delete($node);
      break;

    case 'delete revision':
      taxonomy_node_delete_revision($node);
      break;
  }
}

/**
 * Implementation of hook_form_alter()
 */
function taxonomy_form_alter(&$form, $form_state, $form_id) {
  if (isset($form['type']) && $form['type']['#value'] .'_node_form' == $form_id) {
    // Add own validation handler before any existing ones, to get a chance to
    // modify submitted form data before other modules see it.
    array_unshift($form['#validate'], '_taxonomy_node_form_validate');
  }
}

/**
 * Node form validation handler
 */
function _taxonomy_node_form_validate(&$form, &$form_state) {
  // Inject taxonomy into the form.
  // This is needed for modules such as forum and simplenews,
  // who check for taxonomy during validation and early submission.
  if (empty($form_state['values']['taxonomy']) && !empty($form_state['values']['categories'])) {
    $form_state['values']['taxonomy'] = $form_state['values']['categories'];
  }
}

/**
 * Implementation of hook_help().
 */
function taxonomy_help($section) {
  switch ($section) {
    case 'admin/help#taxonomy':
      return t('<p>This is a wrapper module to provide a compatibility layer between the taxonomy module and the category module. Modules that depend on the taxonomy module should function correctly with this wrapper module enabled, as it routes all taxonomy requests to the category API, and converts category data types into taxonomy data types. The taxonomy module user interface is not available with this wrapper module: you should use the category module user interface instead. For further assistance, see the <a href="!category">category module help page</a>.</p>', array('!category' => url('admin/help/category')));
  }
}

/**
 * Helper function for array_map purposes.
 */
function _taxonomy_get_tid_from_term($term) {
  return $term->tid;
}

/**
 * Converts a category object (as returned by many category API
 * functions) into a term array.
 *
 * @param $node
 *   A category node object, with these properties: nid; title; body;
 *   cnid; and weight (last two are inside $node->category).
 *
 * @return
 *   A corresponding term array, with these properties: tid; vid;
 *   name; description; and weight.
 */
function _taxonomy_category_into_term($node) {
  $term = array();

  $term['tid'] = !empty($node->nid) ? $node->nid : NULL;
  $term['name'] = $node->title;
  $term['description'] = !empty($node->teaser) ? $node->teaser : '';
  $term['vid'] = !empty($node->category['cnid']) ? $node->category['cnid'] : 0;
  $term['weight'] = !empty($node->category['weight']) ? $node->category['weight'] : 0;

  if (!empty($node->category['parents']) && is_array($node->category['parents'])) {
    foreach ($node->category['parents'] as $parent) {
      if (is_numeric($parent)) {
        $parent = category_get_category($parent);
      }
      if (!empty($parent) && is_object($parent)) {
        if ($parent->cnid == $term['vid'] &&
        $parent->cid != $term['vid']) {
          $term['parent'][$parent->cid] = $parent->cid;
        }
      }
    }
  }

  if (empty($term['parent'])) {
    $term['parent'][0] = 0;
  }

  return $term;
}

/**
 * Converts a taxonomy term (as returned by a form submission) into a
 * category node object.
 *
 * @param $term
 *   An array representing a taxonomy term.
 *
 * @return
 *   A corresponding node object representing a category node.
 */
function _taxonomy_term_into_category($term) {
  global $user;
  $node = new stdClass();

  if (!empty($term['tid'])) {
    $node = node_load($term['tid']);
  }

  $container = category_get_container($term['vid']);       
  $node->nid = $term['tid'];
  $node->type = $container->default_category_type;
  $node->title = $term['name'];
  $node->body = $term['description'];
  $node->category['cnid'] = $term['vid'];
  $node->category['weight'] = $term['weight'];
  $node->category['relations'] = $term['relations'];
  $node->category['synonyms'] = $term['synonyms'];
  $node->category['is_legacy'] = TRUE;

  if (!empty($term['parent']) && empty($term['parents'])) {
    $term['parents'] = $term['parent'];
  }
  if (!empty($term['parents']) && is_array($term['parents'])) {
    foreach ($term['parents'] as $parent) {
      if (!empty($parent) && is_numeric($parent)) {
        $node->category['parents'][] = $parent;
      }
    }
  }

  // Force defaults
  $node_options = variable_get('node_options_'. $node->type, array('status', 'promote'));
  $node->status = in_array('status', $node_options);
  $node->promote = in_array('promote', $node_options);
  $node->sticky = in_array('sticky', $node_options);
  $node->revision = in_array('revision', $node_options);
  $node->name = $user->name ? $user->name : 0;
  $node->date = date('j M Y H:i:s');

  return node_submit($node);
}

/**
 * Converts a container node into a vocabulary array.
 *
 * @param $node
 *   A container node object.
 *
 * @return
 *   A corresponding vocabulary array.
 */
function _taxonomy_container_into_vocabulary($node) {
  $edit['vid'] = !empty($node->nid) ? $node->nid : NULL;
  $edit['name'] = $node->title;
  $edit['description'] = $node->body;
  $edit['help'] = $node->category['help'] ? $node->category['help'] : '';
  $edit['multiple'] = $node->category['multiple'];
  $edit['required'] = $node->category['required'];
  if (isset($node->category['hierarchy'])) {
    $edit['hierarchy'] = $node->category['hierarchy'];
  }
  $edit['relations'] = $node->category['relations'];
  $edit['tags'] = $node->category['tags'];
  $edit['weight'] = $node->category['weight'];
  $edit['module'] = isset($node->category['module']) ? $node->category['module'] : 'category';

  if (!isset($node->category['nodes']) || !is_array($node->category['nodes'])) {
    $node->category['nodes'] = array($node->category['nodes']);
  }
  if (empty($node->category['nodes'][0])) {
    unset($node->category['nodes'][0]);
  }
  $edit['nodes'] = $node->category['nodes'];

  return $edit;
}

/**
 * Converts a taxonomy vocabulary (as returned by a form submission) into a
 * container node object.
 *
 * @param $vocabulary
 *   An array representing a taxonomy vocabulary.
 *
 * @return
 *   A corresponding node object representing a container node.
 */
function _taxonomy_vocabulary_into_container($vocabulary) {
  global $user;
  $node = new stdClass();

  if (!empty($vocabulary['vid'])) {
    $node = node_load($vocabulary['vid']);
  }

  $node->nid = $vocabulary['vid'];
  $node->type = 'container';
  $node->title = $vocabulary['name'];
  $node->body = $vocabulary['description'];
  $node->category['help'] = $vocabulary['help'] ? $vocabulary['help'] : '';
  $node->category['multiple'] = $vocabulary['multiple'];
  $node->category['required'] = $vocabulary['required'];
  $node->category['hierarchy'] = $vocabulary['hierarchy'];
  $node->category['relations'] = $vocabulary['relations'];
  $node->category['tags'] = $vocabulary['tags'];
  $node->category['weight'] = $vocabulary['weight'];
  $node->category['module'] = isset($vocabulary['module']) ? $vocabulary['module'] : 'category';
  $node->category['parents'] = array(0 => TRUE);
  $node->category['is_legacy'] = TRUE;

  if (!isset($vocabulary['nodes']) || !is_array($vocabulary['nodes'])) {
    $vocabulary['nodes'] = array($vocabulary['nodes']);
  }
  if (empty($vocabulary['nodes'][0])) {
    unset($vocabulary['nodes'][0]);
  }
  $node->category['nodes'] = $vocabulary['nodes'];

  // Force defaults
  $node_options = variable_get('node_options_'. $node->type, array('status', 'promote'));
  $node->status = in_array('status', $node_options);
  $node->promote = in_array('promote', $node_options);
  $node->sticky = in_array('sticky', $node_options);
  $node->revision = in_array('revision', $node_options);
  $node->name = $user->name ? $user->name : 0;
  $node->date = date('j M Y H:i:s');

  return node_submit($node);
}

/**
 * Function to check the status of the wrapper.
 */
function taxonomy_wrapper_is_enabled() {
  return TRUE;
}
