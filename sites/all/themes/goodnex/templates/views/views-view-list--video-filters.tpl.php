<?php

/**
 * @file views-view-list--video-filters.tpl.php
 * Goodnex's list template file for the video Filters view.
 *
 * - $title : The title of this group of rows.  May be empty.
 * - $options['type'] will either be ul or ol.
 * @ingroup views_templates
 */
?>

<ul id="video-filter" class="video-filter clearfix">
  <li><a data-categories="*"><?php echo t('All'); ?></a></li>
  <?php foreach ($rows as $id => $row): ?>
    <?php print $row; ?>
  <?php endforeach; ?>
</ul>