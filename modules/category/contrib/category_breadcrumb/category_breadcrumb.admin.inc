<?php
// $Id: category_breadcrumb.admin.inc,v 1.1 2009/08/05 04:52:54 jaza Exp $

/**
 * @file
 * Admin page callbacks for the category_breadcrumb module.
 */


/**
 * The admin settings form
 */
function category_breadcrumb_admin_settings() {

  $form['category_breadcrumb_amount'] = array(
    '#type' => 'radios',
    '#title' => t('For categorized nodes'),
    '#default_value' => variable_get('category_breadcrumb_amount', 1),
    '#options' => array(
      '0' => t('Do nothing (keep standard menu-based breadcrumb)'),
      '1' => t('Show just one breadcrumb (default)'),
      '2' => t('Show multiple breadcrumbs'),
      ),
    '#description' => t("While enabled, this feature replaces any existing (menu-based) breadcrumbs on content (node) pages. One breadcrumb is a standard Drupal's format of breadcrumbs, but it can't show multiple paths (for nodes belonging to more categories), and so might fail to provide the expected backwards navigation in some cases. The Multiple breadcrumbs option allows for multiple paths to be shown, but it may be incompatible with some themes.")
  );

  $form['category_breadcrumb_catpages'] = array(
    '#type' => 'radios',
    '#title' => t('For category and container pages'),
    '#default_value' => variable_get('category_breadcrumb_catpages', 1),
    '#options' => array(
      '0' => t('Do nothing (keep standard menu-based breadcrumb)'),
      '1' => t('Show breadcrumb (default)'),
      ),
    '#description' => t('While enabled, this feature replaces any existing (menu-based) breadcrumbs on category and container pages.')
  );

  $form['category_breadcrumb_exclude'] = array(
    '#type' => 'textfield',
    '#title' => t('Do not create breadcrumbs for'),
    '#default_value' => variable_get('category_breadcrumb_exclude', 'forum,blog'),
    '#size' => 60,
    '#maxlength' => 200, 
    '#description' => t("Enter comma-separated list of node-types to exclude from breadcrumb processing. 'forum' and 'blog' are recommended, to avoid compatibility problems. (If needed, names of other node types may be easily observed in HTML source of your pages as class names near top of the content.)")
  );

  $form['category_breadcrumb_title'] = array(
    '#type' => 'radios',
    '#title' => t('Add title of current node to the breadcrumb'),
    '#default_value' => variable_get('category_breadcrumb_title', 0),
    '#options' => array(
      '0' => t('No title, show just path (default)'),
      '1' => t('Title as plain text'),
      '2' => t('Title as link'),
      ),
    '#description' => t("Drupal defines breadcrumbs with no title in it, so usually it's up to theme to add the currently viewed page's title (if desired), and this option should be disabled. But however, while using the Multiple breadcrumbs option, it might be better to enable the title here instead (removing it from your theme), so that the title may be added to each of the multiple paths. You can add title even if no breadcrumb is chosen above (i.e. adding to the standard Drupal's breadcrumb), but this won't work for any pages other than nodes.")
  );

  return system_settings_form($form);
}
