<?php
// $Id: category-display-navigation.tpl.php,v 1.1 2008/07/19 14:56:43 jaza Exp $

/**
 * @file category-display-navigation.tpl.php
 * Default theme implementation to navigate categories. Presented under nodes that
 * are a part of category hierarchies.
 *
 * Available variables:
 * - $tree: The immediate children of the current node rendered as an
 *   unordered list.
 * - $current_depth: Depth of the current node within the category hierarchy.
 *   Provided for context.
 * - $prev_url: URL to the previous node.
 * - $prev_title: Title of the previous node.
 * - $parent_url: URL to the parent node.
 * - $parent_title: Title of the parent node. Not printed by default. Provided
 *   as an option.
 * - $next_url: URL to the next node.
 * - $next_title: Title of the next node.
 * - $has_links: Flags TRUE whenever the previous, parent or next data has a
 *   value.
 * - $container_id: The node ID of the current container being viewed, or of
 *   the container of the currently viewed category. Provided for context.
 * - $container_url: The node URL of the current container hierarchy being viewed.
 *   Provided as an option. Not used by default.
 * - $container_title: The title of the current container hierarchy being viewed.
 *   Provided as an option. Not used by default.
 *
 * @see template_preprocess_category_display_navigation()
 */
?>
<?php if ($tree || $has_links): ?>
  <div id="category-navigation-<?php print $container_id; ?>" class="category-navigation">
    <?php print $tree; ?>

    <?php if ($has_links): ?>
    <div class="page-links clear-block">
      <?php if ($prev_url) : ?>
        <a href="<?php print $prev_url; ?>" class="page-previous" title="<?php print t('Go to previous page'); ?>"><?php print t('‹ ') . $prev_title; ?></a>
      <?php endif; ?>
      <?php if ($parent_url) : ?>
        <a href="<?php print $parent_url; ?>" class="page-up" title="<?php print t('Go to parent page'); ?>"><?php print t('up'); ?></a>
      <?php endif; ?>
      <?php if ($next_url) : ?>
        <a href="<?php print $next_url; ?>" class="page-next" title="<?php print t('Go to next page'); ?>"><?php print $next_title . t(' ›'); ?></a>
      <?php endif; ?>
    </div>
    <?php endif; ?>

  </div>
<?php endif; ?>
