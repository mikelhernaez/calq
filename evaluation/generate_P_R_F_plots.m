function generate_P_R_F_plots(avg_data, avg_size)

metric = {'Recall', 'Precision'};
figure;
for j = 1:length(metric)
    P = avg_data(:,j:3:end);
    plot_joint_PRF(avg_size, P, metric{j}, j)

end

end