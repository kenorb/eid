<?php
/**
 * @file
 * The Flickr API functions.
 */

define('FLICKR_REST_ENDPOINT', 'https://api.flickr.com/services/rest/');
require_once(drupal_get_path('module', 'flickr') . '/flickr.api.inc');

/**
 * A list of possible photo sizes with description and label.
 *
 * @return array
 *   An array of photo sizes.
 */
function flickr_photo_sizes() {
  return array(
    's' => array(
      'label' => 'Square',
      'description' => t('s: 75 px square'),
    ),
    't' => array(
      'label' => 'Thumbnail',
      'description' => t('t: 100px on longest side'),
    ),
    'q' => array(
      'label' => 'Large Square',
      'description' => t('q: 150px square'),
    ),
    'm' => array(
      'label' => 'Small',
      'description' => t('m: 240px on longest side'),
    ),
    'n' => array(
      'label' => 'Small 320',
      'description' => t('n: 320px on longest side'),
    ),
    '-' => array(
      'label' => 'Medium',
      'description' => t('-: 500px on longest side'),
    ),
    'z' => array(
      'label' => 'Medium 640',
      'description' => t('z: 640px on longest side'),
    ),
    'c' => array(
      'label' => 'Medium 800',
      'description' => t('c: 800px on longest side'),
    ),
    'b' => array(
      'label' => 'Large',
      'description' => t('b: 1024px on longest side'),
    ),
    'h' => array(
      'label' => 'Large 1600',
      'description' => t('h: 1600px on longest side'),
    ),
    'k' => array(
      'label' => 'Large 2048',
      'description' => t('k: 2048px on longest side'),
    ),
    'o' => array(
      'label' => 'Original',
      'description' => t('o: Original image'),
    ),
    'x' => array(
      'label' => 'Flash slideshow',
      'description' => t('x: Full featured responsive slideshow (for group, set and user IDs only)'),
    ),
    'y' => array(
      'label' => 'Simple slideshow',
      'description' => t('y: Basic responsive slideshow (for set and user IDs only)'),
    ),
  );
};

/**
 * Submit a request to Flickr.
 *
 * @param string $method
 *   String method name.
 * @param string $args
 *   Associative array of arguments names and values.
 * @param string $cacheable
 *   Boolean indicating if it's safe cache the results of this request.
 * @param string $return_errors
 *   Boolean indicating if the caller will handle displaying error messages.
 *
 * @return array
 *   an array with the the result of the request, or FALSE on error.
 */
function flickr_request($method, $args, $cacheable = TRUE, $return_errors = FALSE) {
  // Add in additional parameters then sort them for signing.
  $args['api_key'] = trim(variable_get('flickr_api_key', ''));
  $args['method'] = $method;
  $args['format'] = 'php_serial';
  ksort($args);

  // Build an argument hash API signing (we'll also use it for the cache id).
  $arg_hash = '';
  foreach ($args as $k => $v) {
    $arg_hash .= $k . $v;
  }
  // If we've got a secret, sign the arguments.
  if ($secret = trim(variable_get('flickr_api_secret', ''))) {
    $args['api_sig'] = md5($secret . $arg_hash);
  }

  // Build the URL.
  foreach ($args as $k => $v) {
    $encoded_params[] = urlencode($k) . '=' . urlencode($v);
  }
  $url = FLICKR_REST_ENDPOINT . '?' . implode('&', $encoded_params);

  // If it's a cachable request, try to load a cached value.
  if ($cacheable) {
    if ($cache = cache_get("flickr_$arg_hash", 'cache')) {
      // Check that the value is still "fresh".
      if ($cache->expire > time()) {
        return unserialize($cache->data);
      }
    }
  }

  // If a cached value wasn't suitable, attempt to connect and fetch a result.
  $result = drupal_http_request($url);
  if ($result->code != 200) {
    if ($return_errors) {
      return array(
        'stat' => 'error',
        // In Drupal <= 5.1, only HTTP errors are stored in $result->code
        // correctly, not TCP/IP errors. We can not count on this variable being
        // correct until this module requires Drupal 5.2 or above.
        'code' => $result->code,
        'message' => $result->error,
      );
    }
    flickr_set_error(t("Could not connect to Flickr, Error: @error", array('@error' => $result->error)));
    return FALSE;
  }

  // Make sure it unserializes.
  $response = unserialize($result->data);
  if (!$response) {
    if ($return_errors) {
      return array(
        'stat' => 'error',
        'code' => '-1',
        'message' => 'The response was corrupted, it could not be unserialized.',
      );
    }
    flickr_set_error(t("Flickr's response was corrupted and could not be unserialized."));
    return FALSE;
  }

  // Check that the request was successful.
  if (flickr_response_has_error($response)) {
    if ($return_errors) {
      return $response;
    }
    flickr_set_error($response);
    return FALSE;
  }

  // Save cacheable results for future use.
  if ($cacheable) {
    cache_set("flickr_$arg_hash", $result->data, 'cache', time() + variable_get('flickr_cache_duration', 3600));
  }

  return $response;
}

/**
 * This function will try to create an html image tag referencing the Flickr
 * photo with the desired size if that size is available for this photo.
 *
 * @param string $photo
 *   The photo variable.
 * @param string $size
 *   The desired image size.
 * @param string $attributes
 *   An optional array of HTML attributes to pass to the image.
 *
 * @return array
 *   a html image tag referencing the image of the desired
 *   size if it is available.
 */
function flickr_img($photo, $size = NULL, $attributes = NULL) {
  $sizes = flickr_photo_sizes();
  if (!isset($size)) {
    $size = '-';
  }
  if (!isset($sizes[$size])) {
    return;
  }
  if (!isset($attributes) || !is_array($attributes)) {
    $attributes = array();
  }
  if (empty($attributes['class'])) {
    $attributes['class'] = NULL;
  }

  // Photoset's use primary instead of id to specify the image.
  if (isset($photo['primary'])) {
    $id = $photo['primary'];
    $attributes['class'] = implode(' ', array(
      $attributes['class'],
      'flickr-photoset-img',
    ));
  }
  else {
    $id = $photo['id'];
    $attributes['class'] = implode(' ', array(
      $attributes['class'],
      'flickr-photo-img',
    ));
  }

  $image_sizes = flickr_photo_get_sizes($id);
  if ($image_sizes) {
    foreach ($image_sizes as $image_size) {
      if ($image_size['label'] == $sizes[$size]['label']) {
        break;
      }
    }
    if (isset($image_size)) {
      $img_url = $image_size['source'];
    }
  }
  else {
    $img_url = flickr_photo_img($photo, $size);
  }
  $info = !is_array($photo['title']) ? flickr_photo_get_info($photo['id']) : '';
  $title = is_array($photo['title']) ? str_replace('"', "'", htmlspecialchars_decode(strip_tags($photo['description']['_content']))) : str_replace('"', "'", htmlspecialchars_decode(strip_tags($info['description']['_content'])));
  if (empty($title) == 'title') {
    $title = $photo['title'];
  }
  $overlay = variable_get('flickr_info_overlay', array(
    'title' => 'title',
    'metadata' => 'metadata',
    'description' => 'description',
    'license' => 0,
  ));
  $title = (gettype($overlay['description']) == 'integer') ? $photo['title'] : $title;
  if (module_exists('jcaption') && (variable_get('flickr_class') != NULL || variable_get('flickr_rel') != NULL)) {
    $title = '';
  }
  return theme('image', array(
    'path' => $img_url,
    'alt' => $title,
    'title' => $title,
    'attributes' => $attributes,
    'getsize' => FALSE,
  ));
}

/**
 * Returns the URL to $photo with size $size using the correct image farm
 * from the $photo variable.
 *
 * @param string $photo
 *   Photo to which the url should point.
 * @param string $size
 *   Size of the photo.
 * @param string $format
 *   Format of the photo.
 *
 * @return array
 *   URL for $photo with the correct size and format.
 */
function flickr_photo_img($photo, $size = NULL, $format = NULL) {
  // Early images don't have a farm setting so default to 1.
  $farm = isset($photo['farm']) ? $photo['farm'] : 1;
  $server = $photo['server'];
  // photoset's use primary instead of id to specify the image.
  $id = isset($photo['primary']) ? $photo['primary'] : $photo['id'];
  $secret = $photo['secret'];
  $suffix = $size ? "_$size." : '.';
  $suffix = $size == '-' ? '.' : $suffix;
  $extension = $size == 'o' ? $format : 'jpg';

  return "https://farm{$farm}.static.flickr.com/{$server}/{$id}_{$secret}" . $suffix . $extension;
}

/**
 * Returns the URL for the Flick photo page.
 *
 * @param string $owner
 *   Owner of the photo.
 * @param string $id
 *   ID of the photo to reference in the URL.
 *
 * @return array
 *   URL for the flickr photo page showing photo with $id of $owner.
 */
function flickr_photo_page_url($owner, $id = NULL) {
  $nsid = is_array($owner) ? $owner['nsid'] : $owner;
  if ($person = flickr_people_get_info($nsid)) {
    return $person['photosurl']['_content'] . $id;
  }
  else {
    return "https://flickr.com/photos/$nsid/$id";
  }
}

/**
 * Returns the URL of a given photoset page.
 *
 * @param string $owner
 *   Owner of the photoset.
 * @param string $id
 *   ID of the photoset to which the url must lead.
 *
 * @return array
 *   URL for the photoset page of photoset $id of owner $owner.
 */
function flickr_photoset_page_url($owner, $id = NULL) {
  $nsid = is_array($owner) ? $owner['nsid'] : $owner;
  if ($person = flickr_people_get_info($nsid)) {
    return $person['photosurl']['_content'] . 'sets/' . $id;
  }
  else {
    return "https://flickr.com/photos/$nsid/sets/$id";
  }
}

/**
 * Tries to match an 'identifier' onto a flickr nsid.
 *
 * This function will first see whether $identifier is already
 * a nsid (format check only, no api call).  If it is not and the
 * identifier has the format of an email, an api call will be made to
 * check whether there is an nsid for that email.  If this is not the
 * case, the $identifier is treated as a username and an api call is
 * made to find the nsid for that username.
 *
 * If none of these succeed, the result will be false
 *
 * @param string $identifier
 *   Identifier to find an NSID for.
 *
 * @return array
 *   Valid NSID or false if none can be found.
 */
function flickr_user_find_by_identifier($identifier) {
  if (flickr_is_nsid($identifier)) {
    // Identifier is an NSID.
    return $identifier;
  }
  if (valid_email_address($identifier) && ($user = flickr_user_find_by_email($identifier))) {
    return $user['nsid'];
  }
  if ($user = flickr_user_find_by_alias($identifier)) {
    return $user['id'];
  }
  if ($user = flickr_user_find_by_username($identifier)) {
    return $user['nsid'];
  }
  return FALSE;
}

/**
 * Flickr is NSID.
 */
function flickr_is_nsid($id) {
  return preg_match('/^\d+@N\d+$/', $id);
}

/**
 * Flickr tag request.
 */
function flickr_tag_request_args($tags = array(), $mode = 'all') {
  $args = array();
  if (!empty($tags)) {
    $args['tags'] = implode(',', $tags);
    $args['tag_mode'] = $mode == 'all' ? $mode : 'any';
  }
  return $args;
}

/**
 * Check if the response from the Flickr api call was an error.
 *
 * @param string $response
 *   Response to check.
 *
 * @return array
 *   True if the response is an error message.
 */
function flickr_response_has_error($response) {
  return !(isset($response['stat']) && $response['stat'] == 'ok');
}

/**
 * Display an error message to flickr admins and write an error to watchdog.
 *
 * @param string $message_or_response
 *   Message or error response to display.
 */
function flickr_set_error($message_or_response) {
  if (is_array($message_or_response)) {
    $message = t('Flickr error @error_id: %flickr_error', array(
      '@error_id' => $message_or_response['code'],
      '%flickr_error' => $message_or_response['message'],
    ));
  }
  else {
    $message = $message_or_response;
  }

  if (user_access('administer flickr')) {
    drupal_set_message($message, 'error');
  }
  watchdog('flickr', $message, array(), WATCHDOG_WARNING);
}

/**
 * Returns Flickr detialed data based on nsid.
 * An elaboration of flickr_people_get_info.
 *
 * @param string $nsid
 *   A valid Flickr numerical user ID. Should be validated before arriving here.
 *
 * @return array $people
 *   HTML with a Flickr user's name that links to their Flickr profile.
 *   Real name if it exists, if not the username.
 */
function flickr_get_user_info($nsid) {
  $info = flickr_people_get_info($nsid);
  $people = array();
  $people['name'] = !empty($info['realname']['_content']) ? l($info['realname']['_content'], $info['profileurl']['_content'], array(
    'attributes' => array(
      'title' => t('View user on Flickr.'),
      'target' => '_blank',
    ),
  )) : l($info['username']['_content'], $info['profileurl']['_content'], array(
    'attributes' => array(
      'title' => t('View user on Flickr.'),
      'target' => '_blank',
    ),
  ));
  $username = strip_tags($people['name']);
  $people['photostream'] = l(t("!user on Flickr", array('!user' => $username)), $info['photosurl']['_content'], array(
    'attributes' => array(
      'title' => t('View Flickr photostream.'),
      'target' => '_blank',
    ),
    'html' => TRUE,
  ));

  $people['photosurl'] = $info['photosurl']['_content'];
  $people['profileurl'] = $info['profileurl']['_content'];
  $people['count'] = $info['photos']['count']['_content'];
  return $people;
}

/**
 * Returns the type of ID. User, group or photoset.
 * If is has no @ it is a set, if flickr_groups_getinfo returns 'ok', it is a
 * group. This is not a validation function.
 *
 * @param string $nsid
 *   A valid Flickr ID. Should be validated before arriving here.
 *
 * @return string
 *   'user', 'group' or 'photoset'
 */
function flickr_get_id_type($id) {
  // If it contains a '@' it is likely a group or user ID.
  if (strpos($id, '@') !== FALSE) {
    $info = flickr_groups_getinfo($id);
    if ($info['stat'] == 'ok') {
      // If the function works, it is a group ID.
      return 'group';
    }
    elseif (flickr_user_find_by_identifier($id)) {
      // If it doesn't, it is likely a user ID.
      return 'user';
    }
  }
  elseif (is_numeric($id)) {
    return 'photoset';
  }
  if (flickr_user_find_by_identifier($id)) {
    return 'user';
  }
  $message = t('A valid Flickr ID could not be found');
  drupal_set_message($message, 'error');
  watchdog('flickr', $message, array(), WATCHDOG_WARNING);
  return FALSE;
}

/**
 * Render multiple photos as an album.
 */
function _flickr_album($type, $id, $show_n, $size, $media, $tags, $tags_user, $delta = NULL, $random = 0, $heading = 'p', $min_title = NULL, $min_metadata = NULL, $vocab_isset = 0) {
  // drupal_is_front_page() is added because it also can have arg(0) == 'node'.
  $cache = cache_get('flickr_block_' . $delta . '_' . $id . '_' . $tags . '_' . arg(0) . '_' . drupal_is_front_page());
  // If $delta is a number it is a block that could be cached.
  if ($cache && is_numeric($delta) && module_exists('flickr_block')) {
    $output = $cache->data;
  }

  // In any other case we generate an album, including a heading and counter.
  else {
    $sizes = flickr_photo_sizes();
    $min_title = is_null($min_title) ? variable_get('flickr_title_suppress_on_small', '100') : $min_title;
    $min_metadata = is_null($min_metadata) ? variable_get('flickr_metadata_suppress_on_small', '150') : $min_metadata;
    if ($show_n > 1 || is_numeric($delta)) {
      $output = '<div class="flickr-photoset">';
    }
    else {
      $output = '<span class="flickr-photoset-single">';
    }
    $recent = ($random == 1 || $id == '39873962@N08') ? '' : t('Recent') . ' ';
    // Merge the user profile terms with the terms from the node.
    $tags_user_string = is_array($tags_user) ? implode(",", $tags_user) : $tags_user;
    $tags_combined = trim($tags . ',' . $tags_user_string, ',');
    // Get information about the album, including the owner if needed later on.
    switch ($type) {
      case "photoset":
        $info = flickr_photoset_get_info($id);
        $people = flickr_get_user_info($info['owner']);
        if (!$info) {
          return;
        }
        break;

      case "group":
        $info = flickr_groups_getinfo($id);
        if (!$info) {
          return;
        }
        break;

      default:
        $people = flickr_get_user_info($id);
        break;
    }

    // Set some variables for all album types.
    if ($show_n > 1) {
      $media_type = $media == 'videos' ? t('videos') : t('photos');
    }
    else {
      $media_type = $media == 'videos' ? t('video') : t('photo');
    }
    $people['photosurl'] = isset($people['photosurl']) ? $people['photosurl'] : 'https://www.flickr.com/groups/' . $id . '/';
    $people['photosurl'] = $id == '39873962@N08' ? 'https://www.flickr.com/search/?sort=relevance&text=' : $people['photosurl'];
    $photos = l(t('@recentFlickr @media', array('@recent' => $recent, '@media' => $media_type)), $people['photosurl'], array(
      'attributes' => array(
        'title' => t('View Flickr photostream.'),
        'target' => '_blank',
      ),
    ));
    // The used tags with a space forced after the comma and the last
    // one replaced by 'or' ('and' for public photos).
    $conjunction = $id == '39873962@N08' ? t('and') : t('or');
    $tags1 = preg_replace('/(.*),/', '$1 ' . $conjunction, str_replace(',', ', ', $tags));
    $tagged = (empty($tags) || $size == 'x' || $size == 'y') ? '' : '<span class="flickr-tags-wrap">' . l(t('tagged') . ' <span class="flickr-tags">' . $tags1 . '</span></span>', $people['photosurl'] . 'tags/' . $tags, array(
      'attributes' => array(
        'title' => t('View tagged photos on Flickr.'),
        'target' => '_blank',
      ),
      'html' => TRUE,
    ));

    // Generate an album heading depending on type.
    switch ($type) {
      case "photoset":
        $set = l($info['title']['_content'], $people['photosurl'] . 'sets/' . $id, array(
          'attributes' => array(
            'title' => t('View set on Flickr.'),
            'target' => '_blank',
          ),
        ));
        $map = variable_get('flickr_maps', 1) ? '(' . l(t('map'), $people['photosurl'] . 'sets/' . $id . '/map/', array(
          'attributes' => array(
            'title' => t('View set on Flickr map.'),
            'target' => '_blank',
          ),
        )) . ')' : '';
        $flickr = l(t('on Flickr'), $people['photosurl'] . 'sets/' . $id, array(
          'attributes' => array(
            'title' => t("@set by @name.", array(
              '@set' => strip_tags($set),
              '@name' => strip_tags($people['name']),
            )),
            'target' => '_blank',
          ),
        ));
        $output .= ($heading == 'none' || ($show_n == 1 && !is_numeric($delta))) ? '' : '<' . $heading . ' class="flickr-album-heading">' . t("!photos !tagsfrom Flickr set !set by !username !map", array(
          '!photos' => $photos,
          '!tags' => $tagged . ' ',
          '!set' => $set,
          '!username' => $people['name'],
          '!map' => $map,
        )) . '</' . $heading . '>';
        break;

      case "group":
        $map = variable_get('flickr_maps', 1) ? '(' . l(t('map'), $people['photosurl'] . 'map/', array(
          'attributes' => array(
            'title' => t('View group photos on Flickr map.'),
            'target' => '_blank',
          ),
         )) . ')' : '';
        $group = l($info['group']['name']['_content'], 'https://www.flickr.com/groups/' . $id . '/', array(
          'attributes' => array(
            'title' => t('View group on Flickr.'),
            'target' => '_blank',
          ),
        ));
        $flickr = l(t('on Flickr'), 'https://www.flickr.com/groups/' . $id . '/', array(
          'attributes' => array(
            'title' => t("Group @name.", array(
              '@name' => strip_tags($info['group']['name']['_content']),
            )),
            'target' => '_blank',
          ),
        ));
        $tags1 = (!empty($tags)) ? '<span class="flickr-tags-wrap">' . t('tagged') . ' <span class="flickr-tags">' . $tags1 . '</span> </span>' : '';
        $output .= ($heading == 'none' || ($show_n == 1 && !is_numeric($delta))) ? '' : '<' . $heading . ' class="flickr-album-heading">' . t("!photos !tags from the group !group !map", array(
          '!photos' => strip_tags($photos),
          '!tags' => $tags1,
          '!group' => $group,
          '!map' => $map,
        )) . '</' . $heading . '>';
        break;

      case "favorites":
        $flickr = l(t('on Flickr'), $people['photosurl'] . 'favorites/', array(
          'attributes' => array(
            'title' => t("View favorites of @name made by others.", array(
              '@name' => strip_tags($people['name']),
            )),
            'target' => '_blank',
          ),
        ));
        $favorites = l(t('favorites'), $people['photosurl'] . 'favorites/', array(
          'attributes' => array(
            'title' => t("View favorites of @name made by others.", array(
              '@name' => strip_tags($people['name']),
            )),
            'target' => '_blank',
          ),
        ));
        $tags1 = (!empty($tags)) ? '<span class="flickr-tags-wrap">' . t('tagged') . ' <span class="flickr-tags">' . $tags1 . '</span> </span>' : '';
        $output .= ($heading == 'none' || ($show_n == 1 && !is_numeric($delta))) ? '' : '<' . $heading . ' class="flickr-album-heading">' . t("!photos !tags from the !favorites of !username", array(
          '!photos' => strip_tags($photos),
          '!tags' => $tags1,
          '!favorites' => $favorites,
          '!username' => $people['name'],
        )) . '</' . $heading . '>';
        break;

      default:
        if (!empty($tags) && strpos($tags, ',') == FALSE) {
          $map = variable_get('flickr_maps', 1) ? '(' . l(t('map'), $people['photosurl'] . 'tags/' . $tags . '/map/', array(
            'attributes' => array(
              'title' => t('View tagged photos on Flickr map.'),
              'target' => '_blank',
            ),
          )) . ')' : '';
        }
        else {
          $map = variable_get('flickr_maps', 1) ? '(' . l(t('map'), $people['photosurl'] . 'map/', array(
            'attributes' => array(
              'title' => t('View user photos on Flickr map.'),
              'target' => '_blank',
            ),
          )) . ')' : '';
        }
        $flickr = (empty($tags)) ? l(t('on Flickr'), $people['photosurl'], array(
          'attributes' => array(
            'title' => t("Photostream by @name.", array(
              '@name' => strip_tags($people['name']),
            )),
            'target' => '_blank',
          ),
        )) : l(t("on Flickr"), $people['photosurl'] . 'tags/' . $tags, array(
          'attributes' => array(
            'title' => t('Tagged') . ' ' . preg_replace('/(.*),/', '$1 ' . $conjunction, str_replace(',', ', ', $tags)) . '.',
            'target' => '_blank',
          ),
          'html' => TRUE,
        ));
        $flickr = $id == '39873962@N08' ? l(t("on Flickr"), $people['photosurl'] . str_replace(",", " ", $tags), array(
          'attributes' => array(
            'title' => t('Tagged') . ' ' . preg_replace('/(.*),/', '$1 ' . $conjunction, str_replace(',', ', ', $tags)) . '.',
            'target' => '_blank',
          ),
          'html' => TRUE,
        )) : $flickr;
        $username = $id == '39873962@N08' ? '' : ' ' . t('by') . ' ' . $people['name'];
        $map = $id == '39873962@N08' ? '' : $map;
        $pre_output = $id == '39873962@N08' ? strip_tags($photos) . ' ' . t('tagged') . ' <span class="flickr-tags">' . $tags1 . '</span> ' . strip_tags($username . ' ' . $map) : $photos . ' '  . $tagged . ' ' . $username . ' ' . $map;
        $pre_output = ($id == '39873962@N08' && empty($tags)) ? 'Provide some tags.' : $pre_output;
        $output .= ($heading == 'none' || ($show_n == 1 && !is_numeric($delta))) ? '' : '<' . $heading . ' class="flickr-album-heading">' . $pre_output . '</' . $heading . '>';
        break;
    }

    // Generate the album itself, depending on size and type.
    switch ($size) {
      case "x":
        switch ($type) {
          case "favorites":
            $output .= t('You cannot choose a slideshow for an album of favorites. Select a size.');
            drupal_set_message(t('You cannot choose a slideshow for an album of favorites.'), 'error');
            break;

          default:
            // Info about set and owner is displayed in slideshow itself. If you
            // want a header nonetheless, use concatenation (.=).
            $output .= theme('flickr_flickrcomslideshow', array(
              'id' => $id,
            ));
            drupal_set_message(t("Note that slideshows show in reverse chronological order (newest first) and ignore number, tags and media settings."), 'warning');
            break;
        }
        break;

      case "y":
        switch ($type) {
          case "favorites":
            $output .= t('You cannot choose a slideshow for an album of favorites. Select a size.');
            drupal_set_message(t('You cannot choose a slideshow for an album of favorites.'), 'error');
            break;

          case "group":
            $output .= t('Choose the Flash slideshow for group pool albums (size = x).');
            drupal_set_message(t('Choose the Flash slideshow for group pool albums (size = x).'), 'error');
            break;

          default:
            $output .= theme('flickr_flickrcomslideshow_simple', array(
              'id' => $id,
            ));
            drupal_set_message(t("Note that slideshows show in reverse chronological order (newest first) and ignore number, tags and media settings."), 'warning');
            break;
        }
        break;

      default:
        $output .= '<div class="flickr-album">';
        // Get a list of "all" the photos. This is cached.
        switch ($type) {
          case "photoset":
            $response = flickr_photosets_getphotos($id, array(
                'per_page' => 500,
                'extras' => 'license,geo,tags,views,media',
                'media' => $media,
            ));
            if (!$response) {
              return;
            }
            $photos = $response['photoset']['photo'];
            break;

          case "group":
            // If only one tag exists, use it to circumvent the 500 limit.
            $tags1 = (strpos($tags, ',')) ? '' : $tags;
            $response = flickr_get_group_photos($id, 1, array(
              'tags' => $tags1,
              'per_page' => 500,
              'extras' => 'license,geo,tags,views,media',
            ));
            if (!$response) {
              return;
            }
            $photos = $response['photos']['photo'];
            break;

          case "favorites":
            $response = flickr_favorites_get_public_list($id, 1, array(
              'per_page' => 500,
              'extras' => 'license,geo,tags,views,media',
            ));
            if (!$response) {
              return;
            }
            $photos = $response['photo'];
            break;

          case "user":
            $response = flickr_photos_search($id, 1, array(
              'tags' => $tags_combined,
              'per_page' => 500,
              'extras' => 'license,geo,tags,views,media',
              'media' => $media,
            ));
            if (!$response) {
              return;
            }
            $photos = $response['photo'];
            break;

          default:
            return;
        }
        if ($random == 1) {
          shuffle($photos);
        }
        // We shouldn't try to return more than the total number of photos.
        $to = min($show_n, count($photos));
        $i = 0;
        $j = 0;
        $k = 0;
        // No Flickr tags specified and on a node page.
        if ($vocab_isset != 0 && empty($tags) && (arg(0) == 'node' && is_numeric(arg(1)) && (arg(2) == '' || arg(2) == 'view'))) {
          $output .= t('Post has no Flickr tags specified.');
        }
        // No Flickr tags specified, not on a node page, thus show all.
        elseif (empty($tags)  && empty($tags_user)) {
          foreach ($photos as $photo) {
            if ($i < $show_n && ($media == 'all' || $media == $photo['media'] . 's')) {
              // Insert owner into $photo because theme_flickr_photo needs it.
              $photo['owner'] = $type == 'photoset' ? $info['owner'] : $photo['owner'];
              // Check if the requested size is available on Flickr.
              if (flickr_in_array_r($sizes[$size]['label'], flickr_photo_get_sizes($photo['id']))) {
                $output .= theme('flickr_photo', array(
                  'photo' => $photo,
                  'size' => $size,
                  'min_title' => $min_title,
                  'min_metadata' => $min_metadata,
                ));
                $i++;
              }
              else {
                $output .= '';
                $k++;
              }
              $j++;
            }
          }
        }
        // Filter on Flickr tags specified in user account.
        elseif (empty($tags)) {
          foreach ($photos as $key => $photo) {
            $tags_user = !is_array($tags_user) ? explode(",", $tags_user) : $tags_user;
            foreach ($tags_user as $term) {
              if ($i >= $show_n && stripos($photo['tags'], $term) !== FALSE) {
                $j++;
              }
              elseif (stripos($photo['tags'], $term) !== FALSE && ($media == 'all' || $media == $photo['media'] . 's')) {
                // Insert owner into $photo because theme_flickr_photo needs it.
                $photo['owner'] = $type == 'photoset' ? $info['owner'] : $photo['owner'];
                // Check if the requested size is available on Flickr.
                if (flickr_in_array_r($sizes[$size]['label'], flickr_photo_get_sizes($photo['id']))) {
                  $output .= theme('flickr_photo', array(
                    'photo' => $photo,
                    'size' => $size,
                    'min_title' => $min_title,
                    'min_metadata' => $min_metadata,
                  ));
                  $i++;
                }
                else {
                  $output .= '';
                  $k++;
                }
                // Avoid this image gets repeated by removing it from the
                // array.
                unset($photos[$key]);
                $j++;
              }
            }
          }
        }
        // Filter on Flickr tags as terms.
        elseif (empty($tags_user) || empty($id)) {
          $terms = is_array($tags) ? $tags : explode(",", $tags);
          $done = 0;
          foreach ($photos as $key => $photo) {
            foreach ($terms as $term) {
              if (($i >= $show_n && stripos($photo['tags'], $term) !== FALSE) || ($photo['id'] == $done && stripos($photo['tags'], $term) !== FALSE)) {
                $j++;
              }
              elseif (stripos($photo['tags'], $term) !== FALSE && ($media == 'all' || $media == $photo['media'] . 's')) {
                // Insert owner into $photo because theme_flickr_photo needs it.
                $photo['owner'] = $type == 'photoset' ? $info['owner'] : $photo['owner'];
                // Check if the requested size is available on Flickr.
                if (flickr_in_array_r($sizes[$size]['label'], flickr_photo_get_sizes($photo['id']))) {
                  $output .= theme('flickr_photo', array(
                    'photo' => $photo,
                    'size' => $size,
                    'min_title' => $min_title,
                    'min_metadata' => $min_metadata,
                  ));
                  $i++;
                }
                else {
                  $output .= '';
                  $k++;
                }
                // Avoid this image gets repeated by removing it from the
                // array AND "remembering" the processed photo ID if it is twice
                // in the array (public photos with multiple tags).
                unset($photos[$key]);
                $done = $photo['id'];
                $j++;
              }
            }
          }
        }
        // Filter on Flickr tags as  terms AND Flickr tags specified in user
        // account.
        else {
          $terms = is_array($tags) ? $tags : explode(",", $tags);
          foreach ($photos as $key => $photo) {
            foreach ($tags_user as $tag) {
              foreach ($terms as $term) {
                if ($i >= $show_n && stripos($photo['tags'], $term) !== FALSE && stripos($photo['tags'], $tag) !== FALSE) {
                  $j++;
                }
                elseif (stripos($photo['tags'], $term) !== FALSE && stripos($photo['tags'], $tag) !== FALSE && ($media == 'all' || $media == $photo['media'] . 's')) {
                  // Insert owner into $photo because theme_flickr_photo needs
                  // it.
                  $photo['owner'] = $type == 'photoset' ? $info['owner'] : $photo['owner'];
                  // Check if the requested size is available on Flickr.
                  if (flickr_in_array_r($sizes[$size]['label'], flickr_photo_get_sizes($photo['id']))) {
                    $output .= theme('flickr_photo', array(
                      'photo' => $photo,
                      'size' => $size,
                      'min_title' => $min_title,
                      'min_metadata' => $min_metadata,
                    ));
                    $i++;
                  }
                  else {
                    $output .= '';
                    $k++;
                  }
                  // Avoid this image gets repeated by removing it from the
                  // array.
                  unset($photos[$key]);
                  $j++;
                }
              }
            }
          }
        }
        if ($k > 0) {
          drupal_set_message($k . ' ' . t("photos were skipped because the requested image size is not available on Flickr (uploaded when this size was not offered yet). To include them, use another size or re-upload these photos on Flickr."), 'warning');
        }
        $output .= '</div>';

        if (module_exists('flickr_block')) {
          // Hide the block if empty if set on the configuration page.
          if ($i == 0 && variable_get('flickr_block_hide_empty', 0)) {
            $output = '';
            return $output;
          }
        }

        // A counter if set to show.
        switch ($type) {
          case "photoset":
            $total = ($media == 'videos') ? $info['count_videos'] : $info['count_photos'] + $info['count_videos'];
            break;

          case "group":
            $total = $info['group']['pool_count']['_content'];
            break;

          case "favorites":
            $total = $response['total'];
            break;

          default:
            $total = $people['count'];
            break;
        }
        $total = (empty($tags)) ? (int) $total : $j;
        $count = t('@number out of @total !flickr', array(
          '@number' => $i,
          '@total' => number_format($total, 0, ",", "."),
          '!flickr' => $flickr,
        ));
        // Note the $delta value is abused for counter override from filter.
        // If the total found > max query (500), omit the counter.
        $terms = isset($terms) ? $terms : 1;
        $output .= (((variable_get('flickr_counter', 1) && $delta != 'false') || $delta == 'true') && $j < (500 * count($terms)) && ($show_n > 1 || is_numeric($delta))) ? '<div class="flickr-album-counter">' . $count . '</div>' : '';
    }
    if ($show_n > 1 || is_numeric($delta)) {
      $output .= '</div>';
    }
    else {
      $output .= '</span>';
    }
    if (is_numeric($delta) && module_exists('flickr_block')) {
      $refresh = $random == 1 ? variable_get('flickr_block_refresh_random', 23) : variable_get('flickr_block_refresh_others', 31);
      // drupal_is_front_page() is added because it also can have
      // arg(0) == 'node'.
      cache_set('flickr_block_' . $delta . '_' . $id . '_' . $tags . '_' . arg(0) . '_' . drupal_is_front_page(), $output, 'cache', time() + $refresh * 60 * 60);
    }
  }
  return $output;
}

/**
 * Returns TRUE if a value is found in a multidimensional array.
 * See http://stackoverflow.com/a/4128377.
 *
 * @param string $needle
 *   The value to be matched.
 *
 * @param array $haystack
 *   The array to match.
 *
 * @param bool $strict
 *   If set to TRUE also check the types of the needle in the haystack.
 *
 * @return bool
 *   TRUE if match found.
 */
function flickr_in_array_r($needle, $haystack, $strict = FALSE) {
  foreach ($haystack as $item) {
    if (($strict ? $item === $needle : $item == $needle) || (is_array($item) && flickr_in_array_r($needle, $item, $strict))) {
      return TRUE;
    }
  }
  return FALSE;
}
