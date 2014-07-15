<?php
// $Id: category_menu.pages.inc,v 1.2 2009/02/07 09:01:16 jaza Exp $

/**
 * @file
 * Page callbacks for the category_menu module.
 */

/**
 * AJAX callback to replace the category_menu 'links for nodes' checkboxes.
 *
 * This function is called when the checkboxes of assigned node types is changed.  It updates the
 * cached form and returns rendered output to be used to replace the checkboxes listing
 * content types that can have menu items generated.
 *
 * @return
 *   Prints the replacement HTML in JSON format.
 */
function category_menu_form_update() {
  $node_types = $_POST['category']['content_types']['nodes'];
  if ($form = form_get_cache($_POST['form_build_id'], $form_state)) {
    $node = $form['#node'];

    // Validate the nodes types.
    if (empty($node_types)) {
      $node_types = array();
    }
    $node_types = array_intersect_key(node_get_types('names'), $node_types);
    $taken_options = array();
    if (empty($node_types)) {
      $node_types = array();
    }
    else {
      foreach ($node_types as $type => $name) {
        $type_setting = variable_get('category_menu_links_'. $type, 0);
        if (!empty($type_setting)) {
          if (!isset($node->nid) || $type_setting != $node->nid) {
            $taken_options[$type] = $type;
          }
        }
      }
    }

    $category_link['links_for_nodes'] = $node_types;
    $category_link['taken_options'] = $taken_options;

    // Get the new options and update the cache.
    $form['category']['menu']['links_for_nodes'] = _category_menu_links_for_nodes($category_link);
    form_set_cache($_POST['form_build_id'], $form, $form_state);

    // Build and render the new checkboxes element, then return it in JSON format.
    $form_state = array();
    $form['#post'] = array();
    $form = form_builder($form['form_id']['#value'] , $form, $form_state);
    $output = drupal_render($form['category']['menu']['links_for_nodes']);
    drupal_json(array('status' => TRUE, 'data' => $output));
  }
  else {
    drupal_json(array('status' => FALSE, 'data' => ''));
  }
  exit();
}

/**
 * AJAX callback to replace the category_menu 'menu link for container' list.
 *
 * This function is called when the menu link toggle checkbox is changed.  It updates the
 * cached form and returns rendered output to be used to replace the list of
 * available parent menu items.
 *
 * @return
 *   Prints the replacement HTML in JSON format.
 */
function category_menu_form_parent_link_update() {
  $category_link['has_parent_link'] = $_POST['category']['menu']['container_has_parent_link'];
  $category_link['has_parents'] = !empty($_POST['category']['hierarchy']['parents']);
  if ($form = form_get_cache($_POST['form_build_id'], $form_state)) {
    $node = $form['#node'];

    if (empty($category_link['has_parent_link'])) {
      $category_link['has_parent_link'] = 0;
    }

    // Get the new options and update the cache.
    $form['category']['menu']['plid_for_container'] = _category_menu_parent_link_for_container($category_link);
    form_set_cache($_POST['form_build_id'], $form, $form_state);

    // Build and render the new checkboxes element, then return it in JSON format.
    $form_state = array();
    $form['#post'] = array();
    $form = form_builder($form['form_id']['#value'] , $form, $form_state);
    $output = drupal_render($form['category']['menu']['plid_for_container']);
    drupal_json(array('status' => TRUE, 'data' => $output));
  }
  else {
    drupal_json(array('status' => FALSE, 'data' => ''));
  }
  exit();
}
