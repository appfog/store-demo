<?php
// $Id: category.admin.inc,v 1.6 2009/12/02 00:45:31 jaza Exp $

/**
 * @file
 * Administrative page callbacks for the category module.
 */

/**
 * Form builder to list and manage containers.
 *
 * @ingroup forms
 * @see category_overview_containers_submit()
 * @see theme_category_overview_containers()
 */
function category_overview_containers(&$form_state) {
  // Check for confirmation forms.
  if (isset($form_state['confirm_reset_alphabetical'])) {
    return category_container_confirm_reset_alphabetical($form_state);
  }

  // Show a warning, if there are no content types with container behavior.
  _category_category_type_options();

  $containers = category_get_containers();
  $form = array('#tree' => TRUE);
  $destination = drupal_get_destination();
  foreach ($containers as $container) {
    $container->parents = category_get_parents($container->cid);
    $container->admin_title = !empty($container->has_admin_title) ? $container->admin_title : '';
    $types = array();
    foreach ($container->nodes as $type) {
      $node_type = node_get_types('name', $type);
      $types[] = $node_type ? check_plain($node_type) : check_plain($type);
    }
    sort($types);
    $types_string = !empty($types) ? implode(', ', $types) : theme('placeholder', t('none'));
    $form[$container->cid]['#container'] = (array)$container;
    $form[$container->cid]['name'] = array('#value' => l($container->title, 'node/'. $container->nid));
    $form[$container->cid]['types'] = array('#value' => $types_string);
    if (variable_get('category_container_hierarchy', 0) > 0) {
      $parents = array();
      foreach ($container->parents as $parent) {
        if ($parent->cid) {
          $parents[] = l($parent->title, 'node/'. $parent->cid);
        }
      }
      $form[$container->cid]['parents'] = array('#value' => implode(', ', $parents));
    }
    if (count($containers) > 1) {
      $form[$container->cid]['weight'] = array('#type' => 'weight', '#delta' => 15, '#default_value' => $container->weight);
    }
    $form[$container->cid]['edit'] = array('#value' => l(t('edit container'), "node/$container->cid/edit", array('query' => $destination)));
    $form[$container->cid]['list'] = array('#value' => l(t('list categories'), "admin/content/category/$container->cid"));
    $form[$container->cid]['add'] = array('#value' => l(t('add categories'), "admin/content/category/$container->cid/add"));
  }

  // Only make this form include a submit button and weight if more than one
  // container exists, and if no containers have multiple parents.
  if (count($containers) > 1) {
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save'),
    );
    $form['reset_alphabetical'] = array(
      '#type' => 'submit',
      '#value' => t('Reset to alphabetical'),
    );
  }
  return $form;
}

/**
 * Submit handler for containers overview. Updates changed container weights.
 *
 * @see category_overview_containers()
 */
function category_overview_containers_submit($form, &$form_state) {
  if ($form_state['clicked_button']['#value'] == t('Reset to alphabetical')) {
    // Execute the reset action.
    if ($form_state['values']['reset_alphabetical'] === TRUE) {
      return category_container_confirm_reset_alphabetical_submit($form, $form_state);
    }
    // Rebuild the form to confirm the reset action.
    $form_state['rebuild'] = TRUE;
    $form_state['confirm_reset_alphabetical'] = TRUE;
    return;
  }

  foreach ($form_state['values'] as $cnid => $container) {
    if (is_numeric($cnid) && $form[$cnid]['#container']['weight'] != $form_state['values'][$cnid]['weight']) {
      $form[$cnid]['#container']['weight'] = $form_state['values'][$cnid]['weight'];
      $node = new StdClass();
      $node->nid = $cnid;
      $node->category = $form[$cnid]['#container'];
      category_save_container($node);
    }
  }
}

/**
 * Theme the container overview as a sortable list of containers.
 *
 * @ingroup themeable
 * @see category_overview_containers()
 */
function theme_category_overview_containers($form) {
  $rows = array();

  $cols = 4;
  if (variable_get('category_container_hierarchy', 0) > 0) {
    $cols++;
  }

  foreach (element_children($form) as $key) {
    if (isset($form[$key]['name'])) {
      $container = &$form[$key];

      $row = array();
      $row[] = drupal_render($container['name']);
      $row[] = drupal_render($container['types']);
      if (variable_get('category_container_hierarchy', 0) > 0) {
        $row[] = drupal_render($container['parents']);
      }
      if (isset($form['submit'])) {
        $container['weight']['#attributes']['class'] = 'container-weight';
        $row[] = drupal_render($container['weight']);
        $cols++;
      }
      $row[] = drupal_render($container['edit']);
      $row[] = drupal_render($container['list']);
      $row[] = drupal_render($container['add']);
      $rows[] = array('data' => $row, 'class' => 'draggable');
    }
  }
  if (empty($rows)) {
    $rows[] = array(array('data' => t('No containers available.'), 'colspan' => $cols));
  }

  $header = array(t('Name'), t('Types'));
  if (variable_get('category_container_hierarchy', 0) > 0) {
    $header[] = t('Parents');
  }
  if (isset($form['submit'])) {
    $header[] = t('Weight');
    drupal_add_tabledrag('category', 'order', 'sibling', 'container-weight');
  }
  $header[] = array('data' => t('Operations'), 'colspan' => '3');
  return theme('table', $header, $rows, array('id' => 'category')) . drupal_render($form);
}

/**
 * Form builder to confirm resetting a container (or all containers) to alphabetical order.
 *
 * @ingroup forms
 * @see category_container_confirm_reset_alphabetical_submit()
 */
function category_container_confirm_reset_alphabetical(&$form_state, $cnid = 0) {
  $container = NULL;
  if (!empty($cnid)) {
    $container = category_get_container($cnid);

    $form['type'] = array('#type' => 'value', '#value' => 'container');
    $form['cnid'] = array('#type' => 'value', '#value' => $cnid);
    $form['title'] = array('#type' => 'value', '#value' => $container->title);
  }
  $form['reset_alphabetical'] = array('#type' => 'value', '#value' => TRUE);
  return confirm_form($form,
                  (!empty($cnid)
                    ? t('Are you sure you want to reset the container %title to alphabetical order?', array('%title' => $container->title))
                    : t('Are you sure you want to reset your containers to alphabetical order?')
                  ),
                  'admin/content/category'. (!empty($cnid) ? '/'. $cnid : ''),
                  (!empty($cnid)
                    ? t('Resetting a container will discard all custom ordering and sort items alphabetically.')
                    : t('Resetting your containers will discard all custom ordering and sort items alphabetically.')
                  ),
                  t('Reset to alphabetical'),
                  t('Cancel'));
}

/**
 * Submit handler to reset a container (or all containers) to alphabetical order after confirmation.
 *
 * @see category_container_confirm_reset_alphabetical()
 */
function category_container_confirm_reset_alphabetical_submit($form, &$form_state) {
  db_query('UPDATE {category} c SET weight = 0 WHERE cnid = %d', $form_state['values']['cnid']);
  if (!empty($form_state['values']['cnid'])) {
    drupal_set_message(t('Reset container %title to alphabetical order.', array('%title' => $form_state['values']['title'])));
    watchdog('category', 'Reset container %title to alphabetical order.', array('%title' => $form_state['values']['title']), WATCHDOG_NOTICE);
    $form_state['redirect'] = 'admin/content/category/'. $form_state['values']['cnid'];
  }
  else {
    drupal_set_message(t('Reset all containers to alphabetical order.'));
    watchdog('category', 'Reset all containers to alphabetical order.', WATCHDOG_NOTICE);
    $form_state['redirect'] = 'admin/content/category';
  }
}

/**
 * Form builder for the over of categories from a particular container.
 *
 * Display a tree of all the categories in a container, with options to edit
 * each one. The form is made drag and drop by the theme function.
 *
 * @ingroup forms
 * @see category_overview_categories_submit()
 * @see theme_category_overview_categories()
 */
function category_overview_categories(&$form_state, $cnid) {
  global $pager_page_array, $pager_total, $pager_total_items;
  $container = category_get_container($cnid);

  // Check for confirmation forms.
  if (isset($form_state['confirm_reset_alphabetical'])) {
    return category_container_confirm_reset_alphabetical($form_state, $container->cid);
  }

  $category_node_types = FALSE;
  foreach (node_get_types() as $type) {
    $behavior = variable_get('category_behavior_'. $type->type, 0);
    if (!empty($behavior) && $behavior == 'category') {
      $category_node_types = TRUE;
    }
  }
  if (!$category_node_types) {
    category_no_node_types('category');
  }

  drupal_set_title(t('Categories in %container', array('%container' => $container->title)));
  $form = array(
    '#container' => (array)$container,
    '#tree' => TRUE,
    '#parent_fields' => FALSE,
  );

  $page            = isset($_GET['page']) ? $_GET['page'] : 0;
  $page_increment  = 25;  // Number of categories per page.
  $page_entries    = 0;   // Elements shown on this page.
  $before_entries  = 0;   // Elements at the root level before this page.
  $after_entries   = 0;   // Elements at the root level after this page.
  $root_entries    = 0;   // Elements at the root level on this page.

  // Categories from previous and next pages are shown if the category tree would have
  // been cut in the middle. Keep track of how many extra categories we show on each
  // page of categories.
  $back_peddle    = NULL;
  $forward_peddle = 0;

  // An array of the categories to be displayed on this page.
  $current_page = array();

  // Case for free tagging.
  if ($container->tags) {
    // We are not calling category_get_tree because that might fail with a big
    // number of tags in the freetagging container.
    $results = pager_query(db_rewrite_sql('SELECT n.nid, n.title, c.*, h.parent FROM {category} c INNER JOIN {node} n ON c.cid = n.nid INNER JOIN {category_hierarchy} h ON c.cid = h.cid WHERE c.cnid = %d ORDER BY c.weight, n.title', 'n', 'nid'), $page_increment, 0, NULL, $container->cid);
    $total_entries = db_query(db_rewrite_sql('SELECT count(*) FROM {category} c INNER JOIN {category_hierarchy} h ON c.cid = h.cid WHERE c.cnid = %d'), $page_increment, 0, NULL, $container->cid);
    while ($category = db_fetch_object($results)) {
      $key = 'cid:'. $category->cid .':0';
      $current_page[$key] = $category;
      $page_entries++;
    }
  }
  // Case for restricted container.
  else {
    $category_deltas = array();
    $tree = category_get_tree($container->cid);
    $category = current($tree);
    $processed_categories = array();
    do {
      // In case this tree is completely empty.
      if (empty($category)) {
        break;
      }
      // $processed_categories is for distant-parent use only: it ensures
      // that categories with multiple distant parents are only printed once,
      // in contrast to categories with multiple local parents.
      if (!empty($container->allowed_parent)) {
        if (isset($processed_categories[$category->cid])) {
          continue;
        }
        else {
          $processed_categories[$category->cid] = TRUE;
        }
      }
      // Count entries before the current page.
      if ($page && ($page * $page_increment) > $before_entries && !isset($back_peddle)) {
        $before_entries++;
        continue;
      }
      // Count entries after the current page.
      elseif ($page_entries > $page_increment && isset($complete_tree)) {
        $after_entries++;
        continue;
      }

      // Do not let a category start the page that is not at the root.
      if (isset($category->depth) && ($category->depth > 0) && !isset($back_peddle)) {
        $back_peddle = 0;
        while ($pcategory = prev($tree)) {
          $before_entries--;
          $back_peddle++;
          if ($pcategory->depth == 0) {
            prev($tree);
            continue 2; // Jump back to the start of the root level parent.
          }
        }
      }
      $back_peddle = isset($back_peddle) ? $back_peddle : 0;

      // Continue rendering the tree until we reach the a new root item.
      if ($page_entries >= $page_increment + $back_peddle + 1 && $category->depth == 0 && $root_entries > 1) {
        $complete_tree = TRUE;
        // This new item at the root level is the first item on the next page.
        $after_entries++;
        continue;
      }
      if ($page_entries >= $page_increment + $back_peddle) {
        $forward_peddle++;
      }

      // Finally, if we've gotten down this far, we're rendering a category on this page.
      $page_entries++;
      $category_deltas[$category->cid] = isset($category_deltas[$category->cid]) ? $category_deltas[$category->cid] + 1 : 0;
      $key = 'cid:'. $category->cid .':'. $category_deltas[$category->cid];

      // Keep track of the first category displayed on this page.
      if ($page_entries == 1) {
        $form['#first_cid'] = $category->cid;
      }
      // Keep a variable to make sure at least 2 root elements are displayed.
      if ($category->parents[0] == 0 || $category->parents[0] == $cnid) {
        $root_entries++;
      }
      $current_page[$key] = $category;
    } while ($category = next($tree));

    // Because we didn't use a pager query, set the necessary pager variables.
    $total_entries = $before_entries + $page_entries + $after_entries;
    $pager_total_items[0] = $total_entries;
    $pager_page_array[0] = $page;
    $pager_total[0] = ceil($total_entries / $page_increment);
  }

  // If this form was already submitted once, it's probably hit a validation
  // error. Ensure the form is rebuilt in the same order as the user submitted.
  if (!empty($form_state['post'])) {
    $order = array_flip(array_keys($form_state['post'])); // Get the $_POST order.
    $current_page = array_merge($order, $current_page); // Update our form with the new order.
    foreach ($current_page as $key => $category) {
      // Verify this is a category for the current page and set at the current depth.
      if (is_array($form_state['post'][$key]) && is_numeric($form_state['post'][$key]['cid'])) {
        $current_page[$key]->depth = $form_state['post'][$key]['depth'];
      }
      else {
        unset($current_page[$key]);
      }
    }
  }

  // Build the actual form.
  foreach ($current_page as $key => $category) {
    // Save the category for the current page so we don't have to load it a second time.
    $form[$key]['#category'] = (array)$category;
    if (isset($category->parents)) {
      $form[$key]['#category']['parent'] = $category->parent = $category->parents[0];
      unset($form[$key]['#category']['parents'], $category->parents);
    }

    if (!$container->tags && ($container->hierarchy < 2 || $container->allowed_parent) && count($tree) > 1) {
      $form['#parent_fields'] = TRUE;
      $form[$key]['cid'] = array(
        '#type' => 'hidden',
        '#value' => $category->cid
      );
      $form[$key]['parent'] = array(
        '#type' => 'hidden',
        // Yes, default_value on a hidden. It needs to be changeable by the javascript.
        '#default_value' => $category->parent,
      );
      $form[$key]['depth'] = array(
        '#type' => 'hidden',
        // Same as above, the depth is modified by javascript, so it's a default_value.
        '#default_value' => $category->depth,
      );
      $form[$key]['title'] = array(
        '#type' => 'textfield',
        '#default_value' => $category->title,
        '#maxlength' => 255,
        '#size' => 40,
      );
    }
    else {
      $form[$key]['title'] = array('#value' => l($category->title, "node/$category->cid"));
    }
    if (!empty($container->allowed_parent)) {
      $parents = array();
      foreach (category_get_parents($category->cid, 'cid', TRUE) as $parent) {
        if (!empty($parent->cid)) {
          $parents[] = l($parent->title, 'node/'. $parent->cid);
        }
      }
      $form[$key]['parents'] = array('#value' => implode(', ', $parents));
    }
    $form[$key]['view'] = array('#value' => l(t('view'), "node/$category->cid"));
    $form[$key]['edit'] = array('#value' => l(t('edit'), "node/$category->cid/edit", array('query' => drupal_get_destination())));
    $form[$key]['delete'] = array('#value' => l(t('delete'), "node/$category->cid/delete", array('query' => drupal_get_destination())));
  }

  $form['#total_entries'] = $total_entries;
  $form['#page_increment'] = $page_increment;
  $form['#page_entries'] = $page_entries;
  $form['#back_peddle'] = $back_peddle;
  $form['#forward_peddle'] = $forward_peddle;
  $form['#empty_text'] = t('No categories available.');

  if (!$container->tags && $container->hierarchy < 2 && count($tree) > 1) {
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save')
    );
    $form['reset_alphabetical'] = array(
      '#type' => 'submit',
      '#value' => t('Reset to alphabetical')
    );
    $form['destination'] = array(
      '#type' => 'hidden',
      '#value' => $_GET['q'] . (isset($_GET['page']) ? '?page='. $_GET['page'] : '')
    );
  }

  return $form;
}

/**
 * Submit handler for categories overview form.
 *
 * Rather than using a textfield or weight field, this form depends entirely
 * upon the order of form elements on the page to determine new weights.
 *
 * Because there might be hundreds or thousands of categories that need to
 * be ordered, categories are weighted from 0 to the number of categories in the
 * container, rather than the standard -10 to 10 scale. Numbers are sorted
 * lowest to highest, but are not necessarily sequential. Numbers may be skipped
 * when a category has children so that reordering is minimal when a child is
 * added or removed from a category.
 *
 * @see category_overview_categories()
 */
function category_overview_categories_submit($form, &$form_state) {
  if ($form_state['clicked_button']['#value'] == t('Reset to alphabetical')) {
    // Execute the reset action.
    if ($form_state['values']['reset_alphabetical'] === TRUE) {
      return category_container_confirm_reset_alphabetical_submit($form, $form_state);
    }
    // Rebuild the form to confirm the reset action.
    $form_state['rebuild'] = TRUE;
    $form_state['confirm_reset_alphabetical'] = TRUE;
    return;
  }

  $order = array_flip(array_keys($form['#post'])); // Get the $_POST order.
  $form_state['values'] = array_merge($order, $form_state['values']); // Update our original form with the new order.

  $container = $form['#container'];
  $hierarchy = 0; // Update the current hierarchy type as we go.

  $changed_categories = array();
  $tree = category_get_tree($container['cid']);

  if (empty($tree)) {
    return;
  }

  // Build a list of all categories that need to be updated on previous pages.
  $weight = 0;
  $category = (array)$tree[0];
  while ($category['cid'] != $form['#first_cid']) {
    if ($category['parents'][0] == 0 && $category['weight'] != $weight) {
      $category['parent'] = $category['parents'][0];
      $category['weight'] = $weight;
      $changed_categories[$category['cid']] = $category;
    }
    $weight++;
    $hierarchy = !category_is_root_parent($category['parents'][0], $container['cid']) ? 1 : $hierarchy;
    $category = (array)$tree[$weight];
  }

  // Renumber the current page weights and assign any new parents.
  $level_weights = array();
  foreach ($form_state['values'] as $cid => $values) {
    if (isset($form[$cid]['#category'])) {
      $category = $form[$cid]['#category'];
      // Give categories at the root level a weight in sequence with categories on previous pages.
      if ($values['parent'] == 0 && $category['weight'] != $weight) {
        $category['weight'] = $weight;
        $changed_categories[$category['cid']] = $category;
      }
      // Categories not at the root level can safely start from 0 because they're all on this page.
      elseif ($values['parent'] > 0) {
        $level_weights[$values['parent']] = isset($level_weights[$values['parent']]) ? $level_weights[$values['parent']] + 1 : 0;
        if ($level_weights[$values['parent']] != $category['weight']) {
          $category['weight'] = $level_weights[$values['parent']];
          $changed_categories[$category['cid']] = $category;
        }
      }
      // Update any changed parents.
      if ($values['parent'] != $category['parent']) {
        $category['parent'] = $values['parent'];
        $changed_categories[$category['cid']] = $category;
      }
      $hierarchy = !category_is_root_parent($category['parent'], $container['cid']) ? 1 : $hierarchy;
      $weight++;

      // Update the title if changed.
      if (!empty($values['title']) && $category['title'] != $values['title']) {
        $category['original_title'] = $category['title'];
        $category['title'] = $values['title'];
        $category['title_changed'] = TRUE;
        $changed_categories[$category['cid']] = $category;
      }
    }
  }

  // Build a list of all categories that need to be updated on following pages.
  for ($weight; $weight < count($tree); $weight++) {
    $category = (array)$tree[$weight];
    if ($category['parents'][0] == 0 && $category['weight'] != $weight) {
      $category['parent'] = $category['parents'][0];
      $category['weight'] = $weight;
      $changed_categories[$category['cid']] = $category;
    }
    $hierarchy = !category_is_root_parent($category['parents'][0], $container['cid']) ? 1 : $hierarchy;
  }

  // Save all updated categories.
  foreach ($changed_categories as $category) {
    $node = new StdClass();
    $node->nid = $category['cid'];
    $node->category = (array) category_get_category($category['cid']);
    $node->category['weight'] = $category['weight'];
    if (empty($container['allowed_parent'])) {
      $parent = empty($category['parent']) ? $category['cnid'] : $category['parent'];
      $node->category['parents'] = array($parent => $parent);
    }
    else {
      $node->category['parents'] = _category_flatten_parents(category_get_parents($category['cid']));
    }
    category_save_category($node);
    // Save the title change.
    if (!empty($category['title_changed'])) {
      $node = node_load($category['cid'], FALSE);
      db_query("UPDATE {node} SET title = '%s' WHERE nid = %d", $category['title'], $category['cid']);
      db_query("UPDATE {node_revisions} SET title = '%s' WHERE vid = %d", $category['title'], $node->vid);
      watchdog('content', 'category: title changed from %original to %current.', array('%original' => $category['original_title'], '%current' => $category['title']), WATCHDOG_NOTICE, l(t('view'), 'node/'. $category['cid']));
    }
  }

  // Update the container hierarchy to flat or single hierarchy.
  if ($container['hierarchy'] != $hierarchy && empty($container['allowed_parent'])) {
    $node = new StdClass();
    $node->nid = $container['cid'];
    $node->category = (array) category_get_container($container['cid']);
    $node->category['parents'] = _category_flatten_parents(category_get_parents($container['cid']));
    $node->category['hierarchy'] = $hierarchy;
    category_save_container($node);
  }

  cache_clear_all();
}

/**
 * Theme the categories overview as a sortable list of categories.
 *
 * @ingroup themeable
 * @see category_overview_categories()
 */
function theme_category_overview_categories($form) {
  $page_increment  = $form['#page_increment'];
  $page_entries    = $form['#page_entries'];
  $back_peddle     = $form['#back_peddle'];
  $forward_peddle  = $form['#forward_peddle'];

  // Add drag and drop if parent fields are present in the form.
  if ($form['#parent_fields']) {
    if (empty($form['#container']['allowed_parent'])) {
      drupal_add_tabledrag('category', 'match', 'parent', 'category-parent', 'category-parent', 'category-id', FALSE);
    }
    drupal_add_tabledrag('category', 'depth', 'group', 'category-depth', NULL, NULL, FALSE);
    drupal_add_js(drupal_get_path('module', 'category') .'/category.js');
    drupal_add_js(array('category' => array('backPeddle' => $back_peddle, 'forwardPeddle' => $forward_peddle)), 'setting');
    drupal_add_css(drupal_get_path('module', 'category') .'/category.css');
  }

  $errors = form_get_errors() != FALSE ? form_get_errors() : array();
  $rows = array();
  foreach (element_children($form) as $key) {
    if (isset($form[$key]['#category'])) {
      $category = &$form[$key];

      $row = array();
      $row[] = (isset($category['#category']['depth']) && $category['#category']['depth'] > 0 ? theme('indentation', $category['#category']['depth']) : '') . drupal_render($category['title']);
      if ($form['#parent_fields']) {
        $category['cid']['#attributes']['class'] = 'category-id';
        $category['parent']['#attributes']['class'] = 'category-parent';
        $category['depth']['#attributes']['class'] = 'category-depth';
        $row[0] .= drupal_render($category['parent']) . drupal_render($category['cid']) . drupal_render($category['depth']);
      }
      if (!empty($form['#container']['allowed_parent'])) {
        $row[] = drupal_render($category['parents']);
      }
      $row[] = drupal_render($category['view']);
      $row[] = drupal_render($category['edit']);
      $row[] = drupal_render($category['delete']);

      $row = array('data' => $row);
      $rows[$key] = $row;
    }
  }

  // Add necessary classes to rows.
  $row_position = 0;
  foreach ($rows as $key => $row) {
    $classes = array();
    if (isset($form['#parent_fields'])) {
      $classes[] = 'draggable';
    }

    // Add classes that mark which categories belong to previous and next pages.
    if ($row_position < $back_peddle || $row_position >= $page_entries - $forward_peddle) {
      $classes[] = 'category-category-preview';
    }

    if ($row_position !== 0 && $row_position !== count($rows) - 1) {
      if ($row_position == $back_peddle - 1 || $row_position == $page_entries - $forward_peddle - 1) {
        $classes[] = 'category-category-divider-top';
      }
      elseif ($row_position == $back_peddle || $row_position == $page_entries - $forward_peddle) {
        $classes[] = 'category-category-divider-bottom';
      }
    }

    // Add an error class if this row contains a form error.
    foreach ($errors as $error_key => $error) {
      if (strpos($error_key, $key) === 0) {
        $classes[] = 'error';
      }
    }
    $rows[$key]['class'] = implode(' ', $classes);
    $row_position++;
  }
  $header = array(t('Name'));
  $cols = 4;
  if (!empty($form['#container']['allowed_parent'])) {
    $header[] = t('Parents');
    $cols++;
  }
  $header[] = array('data' => t('Operations'), 'colspan' => '3');

  if (empty($rows)) {
    $rows[] = array(array('data' => $form['#empty_text'], 'colspan' => $cols));
  }

  $output = theme('table', $header, $rows, array('id' => 'category'));
  $output .= drupal_render($form);
  $output .= theme('pager', NULL, $page_increment);

  return $output;
}

/**
 * Menu callback; presents a list of all node types with necessary behavior,
 * and for each one, outputs a link to its 'node add' page. If there is only
 * one node type with that behavior, then the user is immediately redirected
 * to the 'node add' page for that node type.
 */
function category_add_page($cnid = NULL) {
  if (!isset($cnid)) {
    $behavior = 'container';
    $query = array('destination' => 'admin/content/category');
  }
  else {
    $behavior = 'category';
    $query = array('destination' => 'admin/content/category/'. $cnid, 'parent' => $cnid);
  }
  $types = _category_category_type_options($cnid);

  // For category additions, highlight the default node type selected for
  // this container, and put it to the top of the list.
  if ($behavior == 'category' && $container = category_get_container($cnid)) {
    $default_type = $container->default_category_type;
    if (isset($types[$default_type])) {
      $name = $types[$default_type] .' '. t('(default)');
      unset($types[$default_type]);
      $types = array($default_type => $name) + $types;
    }
  }

  // Build the list of links.
  $items = array();
  $url = $query['destination'];
  foreach ($types as $type => $name) {
    $url = 'node/add/'. str_replace('_', '-', $type);
    $items[] = l($name, $url, array('query' => $query));
  }

  // Redirect if only one item is available, otherwise render the list.
  if (count($items) < 2) {
    drupal_goto($url, $query);
  }
  return theme('item_list', $items, t('Please choose a content type for the new %item:', array('%item' => $behavior)));
}

function category_wrapper_admin_page() {
  $book_status = category_get_wrapper_status('book');
  $taxonomy_status = category_get_wrapper_status('taxonomy');
  $form['book_wrapper'] = array(
    '#type' => 'item',
    '#title' => t('Book wrapper status'),
    '#value' => theme('category_wrapper_status', 'book', $book_status),
  );
  $form['taxonomy_wrapper'] = array(
    '#type' => 'item',
    '#title' => t('Taxonomy wrapper status'),
    '#value' => theme('category_wrapper_status', 'taxonomy', $taxonomy_status),
  );

  return $form;
}

/**
 * Themes the message indicating the status of the specified wrapper module.
 *
 * @param $type
 *   The type of wrapper interface being themed ('taxonomy' or 'book').
 * @param $status
 *   Boolean indicating if the specified wrapper is currently installed.
 *
 * @return
 *   Themed output.
 */
function theme_category_wrapper_status($type, $status) {
  drupal_add_css(drupal_get_path('module', 'category') .'/category.css');
  $output = '<p>';

  if (module_exists($type)) {
    if ($status) {
      $output .= t('The @type wrapper is currently <span class="category-wrapper-installed">installed</span>.', array('@type' => $type)) ."\n";
      $output .= '<br/>'. l(t('Uninstall now'), "category/wrapper/$type/uninstall");
    }
    else {
      $output .= '<p>'. t('The @type wrapper is currently <span class="category-wrapper-notinstalled">not installed</span>.', array('@type' => $type)) ."\n";
      $output .= '<br/>'. l(t('Install now'), "category/wrapper/$type/install");
    }
  }
  else {
    $output .= t('The @type module (original or wrapper) is currently <span class="category-wrapper-notinstalled">not enabled</span>. You must enable it on the <a href="@module-admin-page">module administration page</a> before you can perform an install or an uninstall.', array('@type' => $type, '@module-admin-page' => url('admin/build/modules'))) ."\n";
  }
  $output .= '</p>';
  return $output;
}
