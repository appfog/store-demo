// $Id: category.js,v 1.1 2008/06/14 14:18:46 jaza Exp $

/**
 * Move a category in the categories table from one hierarchy to another via select list.
 *
 * This behavior is dependent on the tableDrag behavior, since it uses the
 * objects initialized in that behavior to update the row.
 */
Drupal.behaviors.categoryDrag = function(context) {
  var table = $('#category', context);
  var tableDrag = Drupal.tableDrag.category; // Get the category tableDrag object.
  var rows = $('tr', table).size();

  // When a row is swapped, keep previous and next page classes set.
  tableDrag.row.prototype.onSwap = function(swappedRow) {
    $('tr.category-category-preview', table).removeClass('category-category-preview');
    $('tr.category-category-divider-top', table).removeClass('category-category-divider-top');
    $('tr.category-category-divider-bottom', table).removeClass('category-category-divider-bottom');

    if (Drupal.settings.category.backPeddle) {
      for (var n = 0; n < Drupal.settings.category.backPeddle; n++) {
        $(table[0].tBodies[0].rows[n]).addClass('category-category-preview');
      }
      $(table[0].tBodies[0].rows[Drupal.settings.category.backPeddle - 1]).addClass('category-category-divider-top');
      $(table[0].tBodies[0].rows[Drupal.settings.category.backPeddle]).addClass('category-category-divider-bottom');
    }

    if (Drupal.settings.category.forwardPeddle) {
      for (var n = rows - Drupal.settings.category.forwardPeddle - 1; n < rows - 1; n++) {
        $(table[0].tBodies[0].rows[n]).addClass('category-category-preview');
      }
      $(table[0].tBodies[0].rows[rows - Drupal.settings.category.forwardPeddle - 2]).addClass('category-category-divider-top');
      $(table[0].tBodies[0].rows[rows - Drupal.settings.category.forwardPeddle - 1]).addClass('category-category-divider-bottom');
    }
  };
};
