<?php
// $Id: category.pages.inc,v 1.7 2009/12/02 00:04:30 jaza Exp $

/**
 * @file
 * Page callbacks for the category module.
 */

/**
 * Menu callback; displays all nodes associated with a category.
 */
function category_page($str_cids = '', $depth = 0, $op = 'page') {
  $categories = category_categories_parse_string($str_cids);
  if ($categories['operator'] != 'and' && $categories['operator'] != 'or') {
    drupal_not_found();
  }

  if ($categories['cids']) {
    $result = db_query(db_rewrite_sql('SELECT n.nid, n.title FROM {node} n WHERE n.nid IN ('. db_placeholders($categories['cids']) .')', 'n', 'nid'), $categories['cids']);
    $cids = array(); // we rebuild the $cids-array so it only contains categories the user has access to.
    $names = array();
    while ($category = db_fetch_object($result)) {
      $cids[] = $category->nid;
      $names[] = $category->title;
    }

    if ($names) {
      $title = check_plain(implode(', ', $names));
      drupal_set_title($title);

      switch ($op) {
        case 'page':
          // Allow category_display to redirect the user to a fully-blown
          // category page, if desired.
          if (module_exists('category_display')) {
            category_display_legacy_page_redirect($cids[0]);
          }

          // Build breadcrumb based on first hierarchy of first category:
          $current->cid = $cids[0];
          $breadcrumb = array();
          while ($parents = category_get_parents($current->cid)) {
            $current = array_shift($parents);
            if (!empty($current->cid)) {
              $breadcrumb[] = l($current->title, 'category/'. $current->cid);
            }
          }
          $breadcrumb[] = l(t('Home'), NULL);
          $breadcrumb = array_reverse($breadcrumb);
          drupal_set_breadcrumb($breadcrumb);

          $output = theme('category_page', $cids, category_select_nodes($cids, $categories['operator'], $depth, TRUE));
          drupal_add_feed(url('category/'. $str_cids .'/'. $depth .'/feed'), 'RSS - '. $title);
          return $output;
          break;

        case 'feed':
          $channel['link'] = url('category/'. $str_cids .'/'. $depth, array('absolute' => TRUE));
          $channel['title'] = variable_get('site_name', 'Drupal') .' - '. $title;
          // Only display the description if we have a single category, to avoid clutter and confusion.
          if (count($cids) == 1) {
            $category = node_load($cids[0]);
            // Always apply proper input format, even if HTML is going to be removed later,
            // so that we don't break the output for complex formatters, and avoid exposure
            // of unfiltered data (such as various pseudo-tags, or even php code).
            $channel['description'] = check_markup($category->teaser, $category->format, FALSE);
          }

          $result = category_select_nodes($cids, $categories['operator'], $depth, FALSE);
          $items = array();
          while ($row = db_fetch_object($result)) {
            $items[] = $row->nid;
          }

          node_feed($items, $channel);
          break;

        default:
          drupal_not_found();
      }
    }
    else {
      drupal_not_found();
    }
  }
}

/**
 * Render a category category page HTML output.
 *
 * @param $cids
 *   An array of category ids.
 * @param $result
 *   A pager_query() result, such as that performed by category_select_nodes().
 *
 * @ingroup themeable
 */
function theme_category_page($cids, $result) {
  drupal_add_css(drupal_get_path('module', 'category') .'/category.css');

  $output = '';

  // Only display the description if we have a single category, to avoid clutter and confusion.
  if (count($cids) == 1) {
    $category = node_load($cids[0]);

    // Check that a description is set.
    if (!empty($category->teaser)) {
      $output .= '<div class="category-category-description">';
      // Always apply proper input format, so that we don't break the output
      // for complex formatters, and avoid exposure of unfiltered data
      // (such as various pseudo-tags, or even php code).
      $output .= check_markup($category->teaser, $category->format, FALSE);
      $output .= '</div>';
    }
  }

  $output .= category_render_nodes($result);

  return $output;
}

/**
 * Menu callback; generates the RSS feed for all nodes associated with a
 * category.
 */
function category_feed($cid) {
  if (is_numeric($cid)) {
    $cat = node_load($cid);
    if (!empty($cat) && !empty($cat->category)) {
      $channel['link'] = url('node/'. $cid, array('absolute' => TRUE));
      $channel['title'] = variable_get('site_name', 'drupal') .' - '. check_plain($cat->title);
      // Always apply proper input format, even if HTML is going to be removed later,
      // so that we don't break the output for complex formatters, and avoid exposure
      // of unfiltered data (such as various pseudo-tags, or even php code).
      $channel['description'] = check_markup($cat->teaser, $cat->format, FALSE);

      if ($cat->category['depth'] < 0) {
        $cat->category['depth'] = 'all';
      }
      $result = category_select_nodes(array($cid), 'or', $cat->category['depth'], FALSE, TRUE);
      $nids = array();
      while ($node = db_fetch_object($result)) {
        $nids[] = $node->nid;
      }
      node_feed($nids, $channel);
    }
  }
}

/**
 * Helper function for autocompletion
 */
function category_autocomplete($cnid, $string = '') {
  // The user enters a comma-separated list of tags. We only autocomplete the last tag.
  $array = drupal_explode_tags($string);

  // Fetch last tag
  $last_string = trim(array_pop($array));
  $matches = array();
  if ($last_string != '') {
    $result = db_query_range(db_rewrite_sql("SELECT n.nid, n.title FROM {node} n INNER JOIN {category} c ON n.nid = c.cid WHERE c.cnid = %d AND LOWER(n.title) LIKE LOWER('%%%s%%')", 'n', 'nid'), $cnid, $last_string, 0, 10);

    $prefix = count($array) ? implode(', ', $array) .', ' : '';

    while ($tag = db_fetch_object($result)) {
      $n = $tag->title;
      // Commas and quotes in terms are special cases, so encode 'em.
      if (strpos($tag->title, ',') !== FALSE || strpos($tag->title, '"') !== FALSE) {
        $n = '"'. str_replace('"', '""', $tag->title) .'"';
      }
      $matches[$prefix . $n] = check_plain($tag->title);
    }
  }

  drupal_json($matches);
}

/**
 * The category wrapper install / uninstall script.
 *
 * @param $type
 *   The wrapper being installed or uninstalled ('taxonomy' or 'book').
 * @param $op
 *   The operation being performed ('install' or 'uninstall').
 */
function category_wrapper($type, $op, $goto = NULL, $rebuild = TRUE) {
  if (!isset($goto)) {
    $goto = 'admin/content/category/wrappers';
  }
  $generic_error = ' '. t('Unable to perform the specified operation.');

  // Various validation checks
  if (!($type == 'taxonomy' || $type == 'book') || !($op == 'install' || $op == 'uninstall')) {
    drupal_set_message(t('Invalid parameters supplied to wrapper install / uninstall script.') . $generic_error, 'error');
    drupal_goto($goto);
  }
  if (!module_exists($type)) {
    drupal_set_message(t('The %type module is not currently enabled. You must enable it before performing an install or uninstall.', array('%type' => $type)), 'error');
    drupal_goto($goto);
  }
  $status = category_get_wrapper_status($type);
  if (($status && $op == 'install') || (!$status && $op == 'uninstall')) {
    drupal_set_message(t('The %type module is already @status.', array('%type' => $type, '@status' => ($status ? t('installed') : t('uninstalled')))) . $generic_error, 'error');
    drupal_goto($goto);
  }

  $module_path = drupal_get_path('module', 'category') .'/wrappers/'. $type;
  $module_file_old = $type .'.module'. ($op == 'install' ? '.php' : '');
  $module_file_old_path = $module_path .'/'. $module_file_old;
  if (!file_exists($module_file_old_path)) {
    drupal_set_message(t('The file %filename could not be found.', array('%filename' => $module_file_old)) . $generic_error, 'error');
    drupal_goto($goto);
  }

  $info_file_old = $type .'.info'. ($op == 'install' ? '.php' : '');
  $info_file_old_path = $module_path .'/'. $info_file_old;
  if (!file_exists($info_file_old_path)) {
    drupal_set_message(t('The file %filename could not be found.', array('%filename' => $info_file_old)) . $generic_error, 'error');
    drupal_goto($goto);
  }
  
  $install_file_old = $type .'.install'. ($op == 'install' ? '.php' : '');
  $install_file_old_path = $module_path .'/'. $install_file_old;
  if (!file_exists($install_file_old_path)) {
    drupal_set_message(t('The file %filename could not be found.', array('%filename' => $install_file_old)) . $generic_error, 'error');
    drupal_goto($goto);
  }

  $module_file_new = $type .'.module'. ($op == 'install' ? '' : '.php');
  $module_file_new_path = $module_path .'/'. $module_file_new;

  $info_file_new = $type .'.info'. ($op == 'install' ? '' : '.php');
  $info_file_new_path = $module_path .'/'. $info_file_new;
  
  $install_file_new = $type .'.install'. ($op == 'install' ? '' : '.php');
  $install_file_new_path = $module_path .'/'. $install_file_new;

  if (!@rename($module_file_old_path, $module_file_new_path)) {
    drupal_set_message(t('The file %filename could not be renamed.', array('%filename' => $module_file_old)) . $generic_error, 'error');
    drupal_goto($goto);
  }

  if (!@rename($info_file_old_path, $info_file_new_path)) {
    drupal_set_message(t('The file %filename could not be renamed.', array('%filename' => $info_file_old)) . $generic_error, 'error');
    drupal_goto($goto);
  }
  
  if (!@rename($install_file_old_path, $install_file_new_path)) {
    drupal_set_message(t('The file %filename could not be renamed.', array('%filename' => $install_file_old)) . $generic_error, 'error');
    drupal_goto($goto);
  }

  if ($type == 'taxonomy' || $type == 'book') {
    if ($op == 'install') {
      db_query("UPDATE {system} SET weight = 10 WHERE name = '%s' AND type = 'module'", $type);
    }
    else {
      db_query("UPDATE {system} SET weight = 0 WHERE name = '%s' AND type = 'module'", $type);
    }
  }

  drupal_set_message(t('The @type wrapper was @op successfully.', array('@type' => $type, '@op' => ($op == 'install' ? t('installed') : t('uninstalled')))));

  if ($rebuild) {
    drupal_flush_all_caches();
    module_rebuild_cache();
  }

  drupal_goto($goto);
}

/**
 * AJAX callback to replace the category parent select options.
 *
 * This function is called when the selected container is changed.  It updates the
 * cached form and returns rendered output to be used to replace the select
 * containing the possible parent pages in the newly selected container.
 *
 * @param $build_id
 *   The form's build_id.
 * @param $container
 *   A container from from among those in the form's container select.
 * @return
 *   Prints the replacement HTML in JSON format.
 */
function category_form_update() {
  $container_nid = $_POST['category']['hierarchy']['container'];

  if ($form = form_get_cache($_POST['form_build_id'], $form_state)) {
    // Validate the container id.
    if (isset($form['category']['hierarchy']['container']['#options'][$container_nid])) {
      $category_link = $form['#node']->category;
      $category_link['container'] = $container_nid;
      if (isset($form['category']['allowed_parents_map']['#value'][$container_nid])) {
        $category_link['allowed_parent'] = $form['category']['allowed_parents_map']['#value'][$container_nid];
      }
      if (isset($form['category']['tags_map']['#value'][$container_nid])) {
        $category_link['tags'] = $form['category']['tags_map']['#value'][$container_nid];
      }
      // Get the new options and update the cache.
      $form['category']['hierarchy']['parents'] = _category_parent_select($category_link);
      form_set_cache($_POST['form_build_id'], $form, $form_state);

      // Build and render the new select element, then return it in JSON format.
      $form_state = array();
      $form['#post'] = array();
      $form = form_builder($form['form_id']['#value'] , $form, $form_state);
      $output = drupal_render($form['category']['hierarchy']['parents']);
      drupal_json(array('status' => TRUE, 'data' => $output));
    }
    else {
      drupal_json(array('status' => FALSE, 'data' => ''));
    }
  }
  else {
    drupal_json(array('status' => FALSE, 'data' => ''));
  }
  exit();
}

/**
 * AJAX callback to replace the category select list for distant children.
 *
 * This function is called when the parent container is changed.  It updates the
 * cached form and returns rendered output to be used to replace the select
 * containing the categories of the child container.
 *
 * @param $parent_cnid
 *   The ID of the parent container.
 * @param $child_cnid
 *   The ID of the child container.
 * @return
 *   Prints the replacement HTML in JSON format.
 */
function category_distant_update($parent_cnid, $child_cnid) {
  $selection_cids = $_POST['categories'][$parent_cnid];

  $parent_cids = array();
  $parent_container = NULL;
  $child_container = NULL;
  $validates = TRUE;

  if (!empty($selection_cids)) {
    if (!is_array($selection_cids)) {
      if (is_numeric($selection_cids)) {
        $parent_cids[] = $selection_cids;
      }
    }
    else {
      $parent_cids = array_filter($selection_cids, 'is_numeric');
    }
  }

  if (empty($parent_cnid) || empty($child_cnid) ||
  !is_numeric($parent_cnid) || !is_numeric($child_cnid)) {
    $validates = FALSE;
  }
  else {
    $parent_container = category_get_container($parent_cnid);
    $child_container = category_get_container($child_cnid);

    if (empty($parent_container) || empty($child_container)) {
      $validates = FALSE;
    }
  }

  if ($form = form_get_cache($_POST['form_build_id'], $form_state)) {

    foreach ($parent_cids as $parent_cid) {
      if (!isset($form['categories'][$parent_cnid]['#options'][$parent_cid])) {
        $validates = FALSE;
        break;
      }
    }

    // Validate the container id.
    if ($validates) {
      // Get the new options and update the cache.
      $form['categories'][$child_cnid] = array_merge(
      $form['categories'][$child_cnid], category_form($child_cnid, 0,
      $child_container->help, 'category', $parent_cids));
      form_set_cache($_POST['form_build_id'], $form, $form_state);

      // Build and render the new select element, then return it in JSON format.
      $form_state = array();
      $form['#post'] = array();
      $form = form_builder($form['form_id']['#value'] , $form, $form_state);
      $output = drupal_render($form['categories'][$child_cnid]);
      drupal_json(array('status' => TRUE, 'data' => $output));
    }
    else {
      drupal_json(array('status' => FALSE, 'data' => ''));
    }
  }
  else {
    drupal_json(array('status' => FALSE, 'data' => ''));
  }
  exit();
}
