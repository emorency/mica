<?php

require_once(drupal_get_path('module', 'mica_datasets') . '/mica_datasets.utils.inc');

function mica_query_crosstab_run_page($dataset, $variable = NULL) {
  ctools_add_css('mica_query', 'mica_query');

  drupal_set_title(t('!title -- Cross tabulation', array('!title' => $dataset->title)));

  $form_state['mica']['dataset_node'] = $dataset;
  $form_state['mica']['variable_node'] = $variable;

  $output = drupal_render(drupal_build_form('mica_query_crosstab_form', $form_state));

  return $output;
}

/*
 * Returns the form to execute a crosstab query
 */
function mica_query_crosstab_form($form, &$form_state){
  $cross_variables = (isset($_REQUEST['cross']) ? $_REQUEST['cross'] : array());

  $dataset = $form_state['mica']['dataset_node'];
  $variable = $form_state['mica']['variable_node'];

  $categorical_variables = mica_datasets_find_categorical_variables($dataset->nid);
  $options = array();
  foreach ($categorical_variables as $v) {
    if (node_access('view', $v)) {
      $options[$v->nid] = $v->title;
    }
  }

  $form['variable'] = array(
    '#type' => 'select',
    '#title' => t('Select a variable'),
    '#options' => $options,
    '#default_value' => isset($variable) ?  $variable->nid : NULL,
    '#empty_options' => -1,
    '#empty_value' => ' - ' . t('None') . ' - ',
  );

  $cross_options = array();
  foreach (mica_dataset_dataset_variables($dataset->nid, TRUE) as $v) {
    if (node_access('view', $v)) {
      $cross_options[$v->nid] = $v->title;
    }
  }

  $form['cross_variable'] = array(
    '#type' => 'select',
    '#multiple' => TRUE,
    '#size' => 6,
    '#title' => t('Cross with'),
    '#options' => $cross_options,
    '#default_value' => empty($cross_variables) ? NULL : $cross_variables,
    '#empty_options' => -1,
    '#empty_value' => ' - ' . t('None') . ' - ',
  );

  $form['submit'] = array(
    '#type' => 'button',
    '#value' => t('Submit'),
    //'#disabled' => TRUE,
    '#ajax' => array(
      'callback' => 'mica_query_crosstab_run_page_submit_callback',
    ),
  );

  $form['results'] = array(
    '#markup' => '', //$result_table,
    '#prefix' => '<div id="result-wrapper">',
    '#suffix' => '</div>',
  );
  return $form;
}
/**
 * Selects just the result table to be returned for re-rendering.
 */
function mica_query_crosstab_run_page_submit_callback($form, $form_state) {

  if (!empty($form_state['values']['cross_variable'])) {
    $dataset_node = $form_state['mica']['dataset_node'];

    $variable_wrapper = entity_metadata_wrapper('node', node_load($form_state['values']['variable']));
    $variable_title = $variable_wrapper->title->value();
    $variable_label = $variable_wrapper->field_label->value();
    $query = _mica_query_crosstab_get_query($dataset_node->nid, $variable_wrapper);

    // Variable description
    $result_table = "<div class='dependent-variable-description'><span class='variable-title'>$variable_title</span>";
    if (!empty($variable_label)){
      $result_table .= " ($variable_label)";
    }
    $result_table .= '</div>';

    foreach ($variable_wrapper->field_variable_categories->value() as $category) {
      if ($category['missing'] == FALSE) {
        $result_table .= $category['name'] . ' : ' . $category['label'] . '<br>';
      }
    }
    // Display a download all results link
    if (count($form_state['values']['cross_variable']) > 1){
      $result_table .= _mica_query_get_xlsx_link($query, $form_state['values']['cross_variable'], 'Download all results as');
    }

    foreach ($variable_wrapper->field_variable_categories->value() as $category) {
      if ($category['missing'] == FALSE) {
        $category_label_by_name[$category['name']] = $category['label'];
      }
    }

    foreach ($form_state['values']['cross_variable'] as $cross_variable_id) {
      $cross_variable = node_load($cross_variable_id);
      $cross_variable_wrapper = entity_metadata_wrapper('node', $cross_variable);
      $cross_title = $cross_variable_wrapper->title->value();

      if ($cross_variable_wrapper->field_value_type->value() === 'text'){
        // Cannot cross with a text variable
        $result_table .= "<div class='crosstab-result'>";
        $result_table .= "Cannot compute cross tabulation of <span class='variable-title'> $variable_title * $cross_title</span>: variable $cross_title is of value type 'TEXT'</div>";
      }
      else {
        $result_table .= "<div class='crosstab-result'>" . t('Cross tabulation results of') .
          "<span class='variable-title'> $variable_title * $cross_title</span>";
        $result_table .= _mica_query_get_xlsx_link($query, array($cross_variable->nid)) . "</div>";
        $result_table .= _mica_query_crosstab_result_table($dataset_node, $query, $cross_variable);
      }
    }
  }
  else{
    $result_table = '<div class="no-results">No \'cross with\' variable(s) have been selected.</div>';
  }

  return array(
    '#type' => 'ajax',
    '#commands' => array(
      ajax_command_html("#result-wrapper", $result_table),
      //ajax_command_html("#cross-variable-detail", _mica_datasets_query_cross_variable_detail_link($cross_variable)),
    )
  );
}

function _mica_query_get_xlsx_link($query, $cross_variable_ids, $title = 'Download as') {
  $xls_url = url('variable-crosstab/' . $query->variable_id . '/xlsx', array(
      'query' => array('cross' => $cross_variable_ids),
    )
  );
  $xls_img_path = drupal_get_path('module', 'mica_query') . '/images/excel.png';
  $xls_img_alt = t('Download as Excel');
  $xls_img = "<img typeof='foaf:Image' class='image-style-none' src='$xls_img_path' alt='$xls_img_alt' title='$xls_img_alt' />";
  $xls_link = " <a href='$xls_url'>". t('!title', array('!title' => $title)) ." $xls_img</a>";
  return  $xls_link;
}

function _mica_query_crosstab_get_query($dataset_id, $variable) {
  $values = array();
  $values['name'] = check_plain('Cross tabulation of ' . $variable->title);
  $values['dataset_id'] = $dataset_id;
  $values['cross_variable_id'] = NULL;
  $values['variable_id'] = $variable->nid->value();
  $values['module'] = 'mica_datasets';
  foreach ($variable->field_variable_categories->value() as $category) {
    if ($category['missing'] === '0') {
      $values['options']['categories'][] = $category['name'];
    }
  }
  $query = MicaDatasetQueryManager::addQuery($values);
  $query->is_new = TRUE;
  MicaDatasetQueryManager::getCache($query)->addTerm($values);

  return $query;
}

function _mica_query_crosstab_result_table($dataset_node, $query, $cross_variable) {
  $output = '';

  $category_label_by_name = array();

  $variable_wrapper = entity_metadata_wrapper('node', $query->variable_id);
  foreach ($variable_wrapper->field_variable_categories->value() as $category) {
    if ($category['missing'] == FALSE) {
      $category_label_by_name[$category['name']] = $category['label'];
    }
  }

  $cross_variable_wrapper = entity_metadata_wrapper('node', $cross_variable);
  $cross_category_label_by_name = array();
  foreach ($cross_variable_wrapper->field_variable_categories->value() as $category) {
    if ($category['missing'] == FALSE) {
      $cross_category_label_by_name[$category['name']] = $category['label'];
    }
  }
  $is_continuous = empty($cross_category_label_by_name);

  $mica_connectors = mica_connector_query_multiple($dataset_node->nid);
  $terms = mica_query_term_query($query);

  $rows = array();

  // Process header
  if (count($mica_connectors) > 0) {
    $headers = _mica_query_crosstab_process_header($category_label_by_name, $terms, $is_continuous);
    $sub_header = _mica_query_crosstab_process_subheader($cross_variable_wrapper, $category_label_by_name, $is_continuous);
    $rows[] = array('data' => $sub_header);
  }

  $study_rowspan = $is_continuous ? 2 : count($cross_category_label_by_name) + 1; // account for N
  $totals = array();
  foreach ($mica_connectors as $connector) {
    $row = array();

    $study = node_load($connector->study_id);
    $study_wrapper = entity_metadata_wrapper('node', $study);
    $acronym = $study_wrapper->field_acroym->value();
    $name = !empty($acronym) ? $study_wrapper->field_acroym->value() : $study_wrapper->title->value();

    $row[] = array(
      'data' => filter_xss($name),
      'class' => array('border-right', 'border-bottom'),
      'rowspan' => $study_rowspan,
    );

    try {
      $result = $connector->crossQueryBy($terms, array($cross_variable->title, $is_continuous));

      foreach ($terms as $term) {

        if (!$is_continuous) {
          // Cross categories
          foreach ($cross_category_label_by_name as $cross_name => $cross_label) {
            $row[] = array(
              'data' => $cross_label . ' (' . $cross_name . ')',
              'class' => array('border-right'),
            );

            $total_per_line = 0;
            foreach ($category_label_by_name as $name => $label) {
              $value = isset($result[$name][$cross_name]) ? $result[$name][$cross_name] : 0;
              $total = isset($result[$name]['_all']) ? $result[$name]['_all'] : 0;
              $value_pct = _mica_query_crosstab_get_percentage($value, $total);
              $row[] = array(
                'data' => $value_pct,
                'title' => $value_pct,
              );

              $total_per_line += $value;
              $totals[$name][$cross_name] = (isset($totals[$name][$cross_name]) ? $totals[$name][$cross_name] + $value: $value);
            }
            $totals[$cross_name]['total'] = (isset($totals[$cross_name]['total']) ? $totals[$cross_name]['total'] + $total_per_line: $total_per_line);
            $value = _mica_query_crosstab_get_percentage($total_per_line, $result['total']['_all']);
            $row[] = array(
              'data' => $value,
              'title' => $value,
            );

            $rows[] = array('data' => $row);
            $row = array();
          }
          // N
          $row[] = array(
            'data' => t('N'),
            'class' => array('border-right', 'border-bottom'),
          );

          $N_total = isset($result['total']['_all']) ? $result['total']['_all'] : 0;
          $totals['N'] = (isset($totals['N']) ? $totals['N'] + $N_total: $N_total);
          foreach ($category_label_by_name as $name => $label) {
            $value = isset($result['_matched'][$name]) ? $result['_matched'][$name] : 0;
            $row[] = array(
              'data' => "$value (100%)",
              'title' => "$value (100%)",
              'class' => array('border-bottom'),
            );
            $totals[$name]['N'] = (isset($totals[$name]['N']) ? $totals[$name]['N'] + $value: $value);
          }
          $row[] = array(
            'data' => "$N_total (100%)",
            'title' => "$N_total (100%)",
            'class' => array('border-bottom'),
          );
          $rows[] = array('data' => $row);
          $row = array();
        }
        else {
          // Crossing with a continuous variable
          $row[] = array(
            'data' => t('Mean (std. deviation)'),
            'class' => array('border-right'),
          );
          foreach ($category_label_by_name as $name => $label) {
            $value = isset($result[$name]['mean']) ? $result[$name]['mean'] : 0;
            $std_dev = isset($result[$name]['std_deviation']) ? $result[$name]['std_deviation'] : 0;
            $row[] = array(
              'data' => round($value, 2) . ' (' . round($std_dev, 2) . ')',
              'title' => $value . ' (' . $std_dev . ')',
            );

            // Info for weighted mean
            $count = isset($result[$name]['count']) ? $result[$name]['count'] : 0;
            $w_mean = $value * $count;
            $totals[$name]['w_mean'] = (isset($totals[$name]['w_mean']) ? $totals[$name]['w_mean'] + $w_mean: $w_mean);
          }
          $row[] = array(
            'data' => '-',
          );
          $rows[] = array('data' => $row);

          // Rows for N
          $row = array();
          $row[] = array(
            'data' => t('N'),
            'class' => array('border-right', 'border-bottom'),
          );
          $N_total = isset($result['total']['_all']) ? $result['total']['_all'] : 0;
          $totals['N'] = (isset($totals['N']) ? $totals['N'] + $N_total: $N_total);
          foreach ($category_label_by_name as $name => $label) {
            $value = isset($result[$name]['count']) ? $result[$name]['count'] : 0;
            //$value_pct = _mica_query_crosstab_get_percentage($value, $N_total);
            $row[] = array(
              'data' => $value,
              'title' => $value,
              'class' => array('border-right', 'border-bottom'),
            );
            $totals[$name]['N'] = (isset($totals[$name]['N']) ? $totals[$name]['N'] + $value: $value);
          }

          $row[] = array(
            'data' => $N_total,
            'title' => $N_total,
            'class' => array('border-bottom'),
          );

          $rows[] = array('data' => $row);
          $row = array();
        }
      }
    } catch (Exception $e) {
      $connector->displayError($e);
    }
  }

  // Add a summary row if there's more than one connector
  if (count($mica_connectors) > 1 && !$is_continuous) {
    $row[] = array(
      'data' => t('All Studies'),
      'class' => array('border-right', 'border-bottom'),
      'rowspan' => $study_rowspan,
    );
    // Cross categories
    foreach ($cross_category_label_by_name as $cross_name => $cross_label) {
      $row[] = array(
        'data' => $cross_label . ' (' . $cross_name . ')',
        'class' =>array('border-right'),
      );

      foreach ($category_label_by_name as $name => $label) {
        $value = _mica_query_crosstab_get_percentage($totals[$name][$cross_name], $totals[$name]['N']);
        $row[] = array(
          'data' => $value,
          'title' => $value,
        );
      }
      // summary total
      $total_pct = _mica_query_crosstab_get_percentage($totals[$cross_name]['total'], $totals['N']);
      $row[] = array(
        'data' => $total_pct,
        'title' => $total_pct,
      );
      $rows[] = array('data' => $row);
      $row = array();
    }
    // N
    $row[] = array(
      'data' => t('N'),
      'class' => array('border-right', 'border-bottom'),
    );

    foreach ($category_label_by_name as $name => $label) {
      $row[] = array(
        'data' => $totals[$name]['N'] . ' (100%)',
        'title' => $totals[$name]['N'] . ' (100%)',
        'class' => array('border-bottom'),
      );
    }
    $row[] = array(
      'data' => $totals['N'] . ' (100%)',
      'title' => $totals['N'] . ' (100%)',
      'class' => array('border-bottom'),
    );

    $rows[] = array('data' => $row);
  }
  else if (count($mica_connectors) > 1 && $is_continuous){
    // Weighted mean
    $row[] = array(
      'data' => t('All Studies'),
      'class' => array('border-right', 'border-bottom'),
      'rowspan' => $study_rowspan,
    );
    $row[] = array(
      'data' => t('Weighted mean'),
      'class' => array('border-right'),
    );
    foreach ($category_label_by_name as $name => $label) {
      $total =  $totals[$name]['N'];
      $value = ($totals[$name]['N'] > 0) ? round($totals[$name]['w_mean'] / $totals[$name]['N'], 2) : 0;
      $row[] = array(
        'data' => $value,
        'title' => $value,
      );
    }
    $row[] = array(
      'data' => '-',
      'class' => array('border-right'),
    );
    $rows[] = array('data' => $row);

    // N
    $row = array();
    $row[] = array(
      'data' => t('N'),
      'class' => array('border-right', 'border-bottom'),
    );
    foreach ($category_label_by_name as $name => $label) {
      $row[] = array(
        'data' => $totals[$name]['N'],
        'title' => $totals[$name]['N'],
        'class' => array('border-right', 'border-bottom'),
      );
    }
    $row[] = array(
      'data' => $totals['N'],
      'class' => array('border-right', 'border-bottom'),
    );
    $rows[] = array('data' => $row);
  }

  $output .= theme(
    'table',
    array(
      'header' => $headers,
      'rows' => $rows,
      'attributes' => array('class' => array('query-table', "continuous_$is_continuous")),
      'empty' => t('No connectors available')
    )
  );

  return $output;
}

function _mica_query_crosstab_process_header($category_label_by_name, $terms, $is_continuous) {
  $header_colspan = count($category_label_by_name);

  // There should always be only one term
  $term = $terms[0];
  // cannot use rowspan for cross queries because of use of <thead> and <tbody>
  $headers = array();
  $headers[] = array(
    'data' => t('Study'),
  );
  // Empty
  $headers[] = array(
    'data' => '', //$cross_variable_wrapper->title->value(),
    'class' => array('border-left', 'border-right')
  );
  // Variable name
  $headers[] = array(
    'data' => $term->termImpl()->getFieldName(),
    'colspan' => $header_colspan,
    'class' => array('border-left', 'border-right'),
  );
  $headers[] = array(
    'data' => t('Total'),
    'class' => array('border-left', 'border-right'),
  );

  return $headers;
}

function _mica_query_crosstab_process_subheader($cross_variable_wrapper, $category_label_by_name, $is_continuous) {
  // empty sub-header
  $sub_header[] = array(
    'data' => ' ',
    'header' => TRUE,
    'class' => 'border-right',
  );
  // sub-header cross-variable name
  $sub_header[] = array(
    'header' => TRUE,
    'class' => array('border-left', 'border-right'),
    'data' => $cross_variable_wrapper->title->value(),
  );
  foreach ($category_label_by_name as $name => $label) {
    $sub_header[] = array(
      'data' => $name,
      'header' => TRUE,
      'title' => $label,
    );
  }
  $sub_header[] = array(
    'data' => '',
    'header' => TRUE,
    'class' => array('border-left', 'border-right'),
  );
  return $sub_header;
}

function _mica_query_crosstab_get_percentage($value, $total) {
  if ($value > 0 && $total > 0) {
    $pct =  round(($value / $total) * 100, 2);
    return "$value ($pct%)";
  }
  return "$value (0%)";
}

function mica_query_crosstab_run_xlsx($variable) {

  $variable1_wrapper = entity_metadata_wrapper('node', $variable);
  $dataset_node = node_load($variable1_wrapper->field_dataset->nid->value());

  $cross_variables = (isset($_REQUEST['cross']) ? $_REQUEST['cross'] : array());
  $filename = $variable1_wrapper->title->value() . ' -- Cross tabulation';

  $prefix = '<html><head><meta http-equiv="Content-Type" content="text/html; charset=utf-8" /></head><body>';
  $suffix = '</body></html>';

  if (!empty($cross_variables)){
    $html = '';
    foreach($cross_variables as $cross_variable_id){
      $variable2 = node_load($cross_variable_id);
      $filename .= ' - ' . $variable2->title;

      $query = _mica_query_crosstab_get_query($dataset_node->nid, $variable1_wrapper);
      $html .= _mica_query_crosstab_result_table($dataset_node, $query, $variable2);
    }
  }
  $filename = truncate_utf8($filename, 255);

  drupal_add_http_header('Content-Type', 'application/vnd.ms-excel');
  drupal_add_http_header('Content-Disposition', "attachment; filename=$filename.xlsx");

  print $prefix . $html . $suffix;
  drupal_exit();
}